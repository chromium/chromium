// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/hdr_copier_layer.h"

#include <CoreGraphics/CoreGraphics.h>
#include <CoreVideo/CoreVideo.h>
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/metal_util/device.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/hdr_metadata_mac.h"

namespace {

// Source of the shader to perform tonemapping. Note that the functions
// ToLinearSRGBIsh, ToLinearPQ, and ToLinearHLG are copy-pasted from the GLSL
// shader source in gfx::ColorTransform.
// TODO(https://crbug.com/1101041): Add non-identity tonemapping to the shader.
NSString* tonemapping_shader_source =
    @"#include <metal_stdlib>\n"
     "#include <simd/simd.h>\n"
     "using metal::float2;\n"
     "using metal::float3;\n"
     "using metal::float3x3;\n"
     "using metal::float4;\n"
     "using metal::sampler;\n"
     "using metal::texture2d;\n"
     "using metal::abs;\n"
     "using metal::exp;\n"
     "using metal::max;\n"
     "using metal::pow;\n"
     "using metal::sign;\n"
     "\n"
     "typedef struct {\n"
     "    float4 clipSpacePosition [[position]];\n"
     "    float2 texcoord;\n"
     "} RasterizerData;\n"
     "\n"
     "float ToLinearSRGBIsh(float v, constant float* gabcdef) {\n"
     "  float g = gabcdef[0];\n"
     "  float a = gabcdef[1];\n"
     "  float b = gabcdef[2];\n"
     "  float c = gabcdef[3];\n"
     "  float d = gabcdef[4];\n"
     "  float e = gabcdef[5];\n"
     "  float f = gabcdef[6];\n"
     "  float abs_v = abs(v);\n"
     "  float sgn_v = sign(v);\n"
     "  if (abs_v < d)\n"
     "    return sgn_v*(c*abs_v + f);\n"
     "  else\n"
     "    return sgn_v*(pow(a*abs_v+b, g) + e);\n"
     "}\n"
     "\n"
     "float ToLinearPQ(float v) {\n"
     "  v = max(0.0f, v);\n"
     "  constexpr float m1 = (2610.0 / 4096.0) / 4.0;\n"
     "  constexpr float m2 = (2523.0 / 4096.0) * 128.0;\n"
     "  constexpr float c1 = 3424.0 / 4096.0;\n"
     "  constexpr float c2 = (2413.0 / 4096.0) * 32.0;\n"
     "  constexpr float c3 = (2392.0 / 4096.0) * 32.0;\n"
     "  float p = pow(v, 1.f / m2);\n"
     "  v = pow(max(p - c1, 0.f) / (c2 - c3 * p), 1.f / m1);\n"
     "  float sdr_white_level = 100.f;\n"
     "  v *= 10000.f / sdr_white_level;\n"
     "  return v;\n"
     "}\n"
     "\n"
     "float ToLinearHLG(float v) {\n"
     "  constexpr float a = 0.17883277;\n"
     "  constexpr float b = 0.28466892;\n"
     "  constexpr float c = 0.55991073;\n"
     "  v = max(0.f, v);\n"
     "  if (v <= 0.5f)\n"
     "    return (v * 2.f) * (v * 2.f);\n"
     "  return exp((v - c) / a) + b;\n"
     "}\n"
     "\n"
     "vertex RasterizerData vertexShader(\n"
     "    uint vertexID [[vertex_id]],\n"
     "    constant float2 *positions[[buffer(0)]]) {\n"
     "  RasterizerData out;\n"
     "  out.clipSpacePosition = vector_float4(0.f, 0.f, 0.f, 1.f);\n"
     "  out.clipSpacePosition.x = 2.f * positions[vertexID].x - 1.f;\n"
     "  out.clipSpacePosition.y = -2.f * positions[vertexID].y + 1.f;\n"
     "  out.texcoord = positions[vertexID];\n"
     "  return out;\n"
     "}\n"
     "\n"
     "float3 ToneMap(float3 v) {\n"
     "  return v;\n"
     "}\n"
     "\n"
     "fragment float4 fragmentShader(RasterizerData in [[stage_in]],\n"
     "                               texture2d<float> t [[texture(0)]],\n"
     "                               constant float3x3& m [[buffer(0)]],\n"
     "                               constant uint32_t& f [[buffer(1)]],\n"
     "                               constant float* gabcdef [[buffer(2)]]) {\n"
     "    constexpr sampler s(metal::mag_filter::nearest,\n"
     "                        metal::min_filter::nearest);\n"
     "    float4 color = t.sample(s, in.texcoord);\n"
     "    switch (f) {\n"
     "      case 1:\n"
     "         color.x = ToLinearSRGBIsh(color.x, gabcdef);\n"
     "         color.y = ToLinearSRGBIsh(color.y, gabcdef);\n"
     "         color.z = ToLinearSRGBIsh(color.z, gabcdef);\n"
     "         break;\n"
     "      case 2:\n"
     "         color.x = ToLinearPQ(color.x);\n"
     "         color.y = ToLinearPQ(color.y);\n"
     "         color.z = ToLinearPQ(color.z);\n"
     "         break;\n"
     "      case 3:\n"
     "         color.x = ToLinearHLG(color.x);\n"
     "         color.y = ToLinearHLG(color.y);\n"
     "         color.z = ToLinearHLG(color.z);\n"
     "         break;\n"
     "      default:\n"
     "         break;\n"
     "    }\n"
     "    color.xyz = ToneMap(m * color.xyz);\n"
     "    return color;\n"
     "}\n";

// Return the integer to use to specify a transfer function to the shader
// defined in the above source. Return 0 if the transfer function is
// unsupported.
uint32_t GetTransferFunctionIndex(const gfx::ColorSpace& color_space) {
  skcms_TransferFunction fn;
  if (color_space.GetTransferFunction(&fn))
    return 1;

  switch (color_space.GetTransferID()) {
    case gfx::ColorSpace::TransferID::PQ:
      return 2;
    case gfx::ColorSpace::TransferID::HLG:
      return 3;
    default:
      return 0;
  }
}

// Convert from an IOSurface's pixel format to a MTLPixelFormat. Crash on any
// unsupported formats. Return true in `is_unorm` if the format, when sampled,
// can produce values outside of [0, 1].
MTLPixelFormat IOSurfaceGetMTLPixelFormat(IOSurfaceRef buffer,
                                          bool* is_unorm = nullptr) {
  uint32_t format = IOSurfaceGetPixelFormat(buffer);
  if (is_unorm)
    *is_unorm = true;
  switch (format) {
    case kCVPixelFormatType_64RGBAHalf:
      if (is_unorm)
        *is_unorm = false;
      return MTLPixelFormatRGBA16Float;
    case kCVPixelFormatType_ARGB2101010LEPacked:
      return MTLPixelFormatBGR10A2Unorm;
    case kCVPixelFormatType_32BGRA:
      return MTLPixelFormatBGRA8Unorm;
    case kCVPixelFormatType_32RGBA:
      return MTLPixelFormatRGBA8Unorm;
    default:
      break;
  }
  return MTLPixelFormatInvalid;
}

base::scoped_nsprotocol<id<MTLRenderPipelineState>> CreateRenderPipelineState(
    id<MTLDevice> device) {
  base::scoped_nsprotocol<id<MTLRenderPipelineState>> render_pipeline_state;

  base::scoped_nsprotocol<id<MTLLibrary>> library;
  {
    NSError* error = nil;
    base::scoped_nsobject<MTLCompileOptions> options(
        [[MTLCompileOptions alloc] init]);
    library.reset([device newLibraryWithSource:tonemapping_shader_source
                                       options:options
                                         error:&error]);
    if (error) {
      NSLog(@"Failed to compile shader: %@", error);
      return render_pipeline_state;
    }
  }

  {
    base::scoped_nsprotocol<id<MTLFunction>> vertex_function(
        [library newFunctionWithName:@"vertexShader"]);
    base::scoped_nsprotocol<id<MTLFunction>> fragment_function(
        [library newFunctionWithName:@"fragmentShader"]);
    NSError* error = nil;
    base::scoped_nsobject<MTLRenderPipelineDescriptor> desc(
        [[MTLRenderPipelineDescriptor alloc] init]);
    [desc setVertexFunction:vertex_function];
    [desc setFragmentFunction:fragment_function];
    [[desc colorAttachments][0] setPixelFormat:MTLPixelFormatRGBA16Float];
    render_pipeline_state.reset(
        [device newRenderPipelineStateWithDescriptor:desc error:&error]);
    if (error) {
      NSLog(@"Failed to create render pipeline state: %@", error);
      return render_pipeline_state;
    }
  }

  return render_pipeline_state;
}

}  // namespace

API_AVAILABLE(macos(10.15))
@interface HDRCopierLayer : CAMetalLayer {
  base::scoped_nsprotocol<id<MTLRenderPipelineState>> _renderPipelineState;
  gfx::ColorSpace _colorSpace;
  absl::optional<gfx::HDRMetadata> _hdrMetadata;
}
- (id)init;
- (void)setHDRContents:(IOSurfaceRef)buffer
            withDevice:(id<MTLDevice>)device
        withColorSpace:(gfx::ColorSpace)colorSpace
          withMetadata:(absl::optional<gfx::HDRMetadata>)hdrMetadata;
@end

@implementation HDRCopierLayer
- (id)init {
  if (self = [super init]) {
    base::scoped_nsprotocol<id<MTLDevice>> device(metal::CreateDefaultDevice());
    if (@available(macOS 10.11, iOS 16.0, *)) {
      [self setWantsExtendedDynamicRangeContent:YES];
    }
    [self setDevice:device];
    [self setOpaque:NO];
    [self setPresentsWithTransaction:YES];
    [self setPixelFormat:MTLPixelFormatRGBA16Float];
    base::ScopedCFTypeRef<CGColorSpaceRef> colorSpace(
        CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB));
    [self setColorspace:colorSpace];
  }
  return self;
}

- (void)setHDRContents:(IOSurfaceRef)buffer
            withDevice:(id<MTLDevice>)device
        withColorSpace:(gfx::ColorSpace)colorSpace
          withMetadata:(absl::optional<gfx::HDRMetadata>)hdrMetadata {
  // Retrieve information about the IOSurface.
  size_t width = IOSurfaceGetWidth(buffer);
  size_t height = IOSurfaceGetHeight(buffer);
  MTLPixelFormat mtlFormat = IOSurfaceGetMTLPixelFormat(buffer);
  if (mtlFormat == MTLPixelFormatInvalid) {
    DLOG(ERROR) << "Unsupported IOSurface format.";
    return;
  }

  if (@available(macOS 10.15, iOS 16.0, *)) {
    // Set metadata for tone mapping.
    if (_colorSpace != colorSpace || _hdrMetadata != hdrMetadata) {
      CAEDRMetadata* edrMetadata = nil;
      switch (colorSpace.GetTransferID()) {
        case gfx::ColorSpace::TransferID::PQ: {
          base::ScopedCFTypeRef<CFDataRef> displayInfo;
          base::ScopedCFTypeRef<CFDataRef> contentInfo;
          displayInfo = gfx::GenerateMasteringDisplayColorVolume(hdrMetadata);
          contentInfo = gfx::GenerateContentLightLevelInfo(hdrMetadata);
          edrMetadata = [CAEDRMetadata
              HDR10MetadataWithDisplayInfo:base::mac::CFToNSCast(displayInfo)
                               contentInfo:base::mac::CFToNSCast(contentInfo)
                        opticalOutputScale:100];
          break;
        }
        case gfx::ColorSpace::TransferID::HLG:
          edrMetadata = [CAEDRMetadata HLGMetadata];
          break;
        default:
          [self setEDRMetadata:nil];
          break;
      }
      [self setEDRMetadata:edrMetadata];
      _colorSpace = colorSpace;
      _hdrMetadata = hdrMetadata;
    }
  }
  // Migrate to the MTLDevice on which the CAMetalLayer is being composited, if
  // known.
  if (device) {
    [self setDevice:device];
  } else {
    if (@available(macOS 10.15, *)) {
      id<MTLDevice> preferredDevice = [self preferredDevice];
      if (preferredDevice) {
        [self setDevice:preferredDevice];
      }
    }
    device = [self device];
  }

  // When the device changes, rebuild the RenderPipelineState.
  if (device != [_renderPipelineState device]) {
    _renderPipelineState = CreateRenderPipelineState(device);
  }
  if (!_renderPipelineState) {
    return;
  }

  // Update the layer's properties to match the IOSurface.
  [self setDrawableSize:CGSizeMake(width, height)];

  // Create a texture to wrap the IOSurface.
  base::scoped_nsprotocol<id<MTLTexture>> bufferTexture;
  {
    base::scoped_nsobject<MTLTextureDescriptor> texDesc(
        [MTLTextureDescriptor new]);
    [texDesc setTextureType:MTLTextureType2D];
    [texDesc setUsage:MTLTextureUsageShaderRead];
    [texDesc setPixelFormat:mtlFormat];
    [texDesc setWidth:width];
    [texDesc setHeight:height];
    [texDesc setDepth:1];
    [texDesc setMipmapLevelCount:1];
    [texDesc setArrayLength:1];
    [texDesc setSampleCount:1];
#if BUILDFLAG(IS_MAC)
    [texDesc setStorageMode:MTLStorageModeManaged];
#endif
    bufferTexture.reset([device newTextureWithDescriptor:texDesc
                                               iosurface:buffer
                                                   plane:0]);
  }

  // Create a texture to wrap the drawable.
  id<CAMetalDrawable> drawable = [self nextDrawable];
  id<MTLTexture> drawableTexture = [drawable texture];

  // Copy from the IOSurface to the drawable.
  base::scoped_nsprotocol<id<MTLCommandQueue>> commandQueue(
      [device newCommandQueue]);
  id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
  id<MTLRenderCommandEncoder> encoder = nil;
  {
    MTLRenderPassDescriptor* desc =
        [MTLRenderPassDescriptor renderPassDescriptor];
    desc.colorAttachments[0].texture = drawableTexture;
    desc.colorAttachments[0].loadAction = MTLLoadActionClear;
    desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
    encoder = [commandBuffer renderCommandEncoderWithDescriptor:desc];

    MTLViewport viewport;
    viewport.originX = 0;
    viewport.originY = 0;
    viewport.width = width;
    viewport.height = height;
    viewport.znear = -1.0;
    viewport.zfar = 1.0;
    [encoder setViewport:viewport];
    [encoder setRenderPipelineState:_renderPipelineState];
    [encoder setFragmentTexture:bufferTexture atIndex:0];
  }
  {
    simd::float2 positions[6] = {
        simd::make_float2(0, 0), simd::make_float2(0, 1),
        simd::make_float2(1, 1), simd::make_float2(1, 1),
        simd::make_float2(1, 0), simd::make_float2(0, 0),
    };

    // The value of |transfer_function| corresponds to the value as used in
    // the above shader source.
    uint32_t transferFunctionIndex = GetTransferFunctionIndex(colorSpace);
    DCHECK(transferFunctionIndex);

    skcms_TransferFunction fn;
    colorSpace.GetTransferFunction(&fn);

    // Matrix is the primary transform matrix from |color_space| to sRGB.
    simd::float3x3 matrix;
    {
      skcms_Matrix3x3 src_to_xyz;
      skcms_Matrix3x3 srgb_to_xyz;
      skcms_Matrix3x3 xyz_to_srgb;
      colorSpace.GetPrimaryMatrix(&src_to_xyz);
      gfx::ColorSpace::CreateSRGB().GetPrimaryMatrix(&srgb_to_xyz);
      skcms_Matrix3x3_invert(&srgb_to_xyz, &xyz_to_srgb);
      skcms_Matrix3x3 m = skcms_Matrix3x3_concat(&xyz_to_srgb, &src_to_xyz);
      matrix = simd::float3x3(
          simd::make_float3(m.vals[0][0], m.vals[1][0], m.vals[2][0]),
          simd::make_float3(m.vals[0][1], m.vals[1][1], m.vals[2][1]),
          simd::make_float3(m.vals[0][2], m.vals[1][2], m.vals[2][2]));
    }

    [encoder setVertexBytes:positions length:sizeof(positions) atIndex:0];
    [encoder setFragmentBytes:&matrix length:sizeof(matrix) atIndex:0];
    [encoder setFragmentBytes:&transferFunctionIndex
                       length:sizeof(transferFunctionIndex)
                      atIndex:1];
    [encoder setFragmentBytes:&fn length:sizeof(fn) atIndex:2];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                vertexStart:0
                vertexCount:6];
  }
  [encoder endEncoding];
  [commandBuffer commit];
  [commandBuffer waitUntilScheduled];
  [drawable present];
}
@end

namespace metal {

CALayer* CreateHDRCopierLayer() {
  // If this is hit by non-10.15 paths (e.g, for testing), then return an
  // ordinary CALayer. Calling setContents on that CALayer will work fine
  // (HDR content will be clipped, but that would have happened anyway).
  if (@available(macos 10.15, *))
    return [[HDRCopierLayer alloc] init];
  NOTREACHED();
  return nil;
}

void UpdateHDRCopierLayer(
    CALayer* layer,
    IOSurfaceRef buffer,
    id<MTLDevice> device,
    const gfx::ColorSpace& color_space,
    const absl::optional<gfx::HDRMetadata>& hdr_metadata) {
  if (@available(macos 10.15, *)) {
    if (auto* hdr_copier_layer = base::mac::ObjCCast<HDRCopierLayer>(layer)) {
      [hdr_copier_layer setHDRContents:buffer
                            withDevice:device
                        withColorSpace:color_space
                          withMetadata:hdr_metadata];
      return;
    }
  }
  NOTREACHED();
}

bool ShouldUseHDRCopier(IOSurfaceRef buffer,
                        gfx::HDRMode hdr_mode,
                        const gfx::ColorSpace& color_space) {
  if (@available(macos 10.15, *)) {
    // Only some transfer functions are supported.
    if (!GetTransferFunctionIndex(color_space))
      return false;

    // Only some pixel formats are supported.
    bool is_unorm = false;
    if (IOSurfaceGetMTLPixelFormat(buffer, &is_unorm) == MTLPixelFormatInvalid)
      return false;

    if (color_space.IsToneMappedByDefault())
      return true;

    if (hdr_mode == gfx::HDRMode::kDefault) {
      if (color_space.GetTransferID() ==
          gfx::ColorSpace::TransferID::SRGB_HDR) {
        // Rasterized tiles and the primary plane specify a color space of
        // SRGB_HDR with gfx::HDRMode::kDefault.
        return !is_unorm;
      }
      return false;
    }
    return true;
  }
  return false;
}

}  // namespace metal
