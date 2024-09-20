// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/metal_util/hdr_copier_layer.h"

#include <CoreGraphics/CoreGraphics.h>
#include <CoreVideo/CoreVideo.h>
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/feature_list.h"
#include "base/strings/sys_string_conversions.h"
#include "components/metal_util/device.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/hdr_metadata_mac.h"

namespace {

// If true, then use the HDRCopierLayer for all HLG video content.
BASE_FEATURE(kMacHlgUseHdrCopier,
             "MacHlgUseHdrCopier",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Source of the shader to perform tonemapping. Note that the functions
// ToLinearSRGBIsh, ToLinearPQ, and ToLinearHLG are copy-pasted from the GLSL
// shader source in gfx::ColorTransform.
// TODO(crbug.com/40138176): Add non-identity tonemapping to the shader.
NSString* tonemapping_shader_source =
    @"#include <metal_stdlib>\n"
     "#include <simd/simd.h>\n"
     "using metal::float2;\n"
     "using metal::float3;\n"
     "using metal::float3x3;\n"
     "using metal::float4x4;\n"
     "using metal::float4;\n"
     "using metal::sampler;\n"
     "using metal::texture2d;\n"
     "using metal::abs;\n"
     "using metal::dot;\n"
     "using metal::exp;\n"
     "using metal::exp2;\n"
     "using metal::log;\n"
     "using metal::log2;\n"
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
     "  return v;\n"
     "}\n"
     "\n"
     "float ToLinearHLG(float v) {\n"
     "  constexpr float a = 0.17883277;\n"
     "  constexpr float b = 0.28466892;\n"
     "  constexpr float c = 0.55991073;\n"
     "  v = max(0.f, v);\n"
     "  if (v <= 0.5f)\n"
     "    return v * v / 3.f;\n"
     "  return (exp((v - c) / a) + b) / 12.f;\n"
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
     "fragment float4 fragmentShader(\n"
     "        RasterizerData in [[stage_in]],\n"
     "        texture2d<float> plane0 [[texture(0)]],\n"
     "        texture2d<float> plane1 [[texture(1)]],\n"
     "        constant float4x4& yuvToRgb [[buffer(0)]],\n"
     "        constant float3x3& primaryMatrix [[buffer(1)]],\n"
     "        constant uint32_t& numPlanes [[buffer(2)]],\n"
     "        constant uint32_t& trfnId [[buffer(3)]],\n"
     "        constant float* gabcdef [[buffer(4)]],\n"
     "        constant float& screenHdrHeadroom [[buffer(5)]]) {\n"
     "    constexpr sampler s(metal::mag_filter::nearest,\n"
     "                        metal::min_filter::nearest);\n"
     "    float4 color = plane0.sample(s, in.texcoord);\n"
     "    if (numPlanes >= 2) {\n"
     "        color.yz = plane1.sample(s, in.texcoord).xy;\n"
     "        color.w = 1.0;\n"
     "    }\n"
     "    if (color.w != 0.0) {\n"
     "      color.xyz /= color.w;\n"
     "    }\n"
     "    color = yuvToRgb * color;\n"
     "    switch (trfnId) {\n"
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
     "    color.xyz = primaryMatrix * color.xyz;\n"
     "    switch (trfnId) {\n"
     "      case 2:\n"
     "         // Scale to be relative to 203 nits.\n"
     "         color.xyz *= 10000.f / 203.f;\n"
     "         break;\n"
     "      case 3: {\n"
     "         // Compute the gain for the ITU-R BT.2100 Reference OOTF.\n"
     "         const float3 Y = float3(0.2627f, 0.6780f, 0.0593f);\n"
     "         float gain = max(pow(dot(Y, color.xyz), 0.2), 1.0);\n"
     "         // Add the gain to scale to be relative to 203 nits.\n"
     "         gain *= 1000.f / 203.f;\n"
     "         // Compute the gain weight;\n"
     "         const float contentHdrHeadroom = 1000.f/203.f;\n"
     "         const float weight = log2(screenHdrHeadroom) /\n"
     "                              log2(contentHdrHeadroom);\n"
     "         // Apply the gain.\n"
     "         color.xyz *= exp2(weight * log2(gain));\n"
     "         break;\n"
     "      }\n"
     "      default:\n"
     "         break;\n"
     "    }\n"
     "    color.xyz *= color.w;\n"
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

// Convert from an IOSurface's pixel format to a MTLPixelFormat. Return true in
// `is_unorm` if the format, when sampled, can produce values outside of [0, 1].
bool IOSurfaceGetMTLPixelFormat(IOSurfaceRef buffer,
                                uint32_t& num_planes,
                                MTLPixelFormat format[2],
                                bool& is_unorm) {
  num_planes = 1;
  format[0] = MTLPixelFormatInvalid;
  format[1] = MTLPixelFormatInvalid;
  is_unorm = true;
  switch (IOSurfaceGetPixelFormat(buffer)) {
    case kCVPixelFormatType_64RGBAHalf:
      is_unorm = false;
      format[0] = MTLPixelFormatRGBA16Float;
      return true;
    case kCVPixelFormatType_ARGB2101010LEPacked:
      format[0] = MTLPixelFormatBGR10A2Unorm;
      return true;
    case kCVPixelFormatType_32BGRA:
      format[0] = MTLPixelFormatBGRA8Unorm;
      return true;
    case kCVPixelFormatType_32RGBA:
      format[0] = MTLPixelFormatRGBA8Unorm;
      return true;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
      num_planes = 2;
      format[0] = MTLPixelFormatR8Unorm;
      format[1] = MTLPixelFormatRG8Unorm;
      return true;
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
    case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
    case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
      num_planes = 2;
      format[0] = MTLPixelFormatR16Unorm;
      format[1] = MTLPixelFormatRG16Unorm;
      return true;
    default:
      break;
  }
  return false;
}

id<MTLRenderPipelineState> CreateRenderPipelineState(id<MTLDevice> device) {
  NSError* error = nil;
  id<MTLLibrary> library =
      [device newLibraryWithSource:tonemapping_shader_source
                           options:[[MTLCompileOptions alloc] init]
                             error:&error];
  if (error) {
    NSLog(@"Failed to compile shader: %@", error);
    return nil;
  }

  MTLRenderPipelineDescriptor* desc =
      [[MTLRenderPipelineDescriptor alloc] init];
  desc.vertexFunction = [library newFunctionWithName:@"vertexShader"];
  desc.fragmentFunction = [library newFunctionWithName:@"fragmentShader"];
  desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
  id<MTLRenderPipelineState> render_pipeline_state =
      [device newRenderPipelineStateWithDescriptor:desc error:&error];
  if (error) {
    NSLog(@"Failed to create render pipeline state: %@", error);
    return nil;
  }

  return render_pipeline_state;
}

}  // namespace

@interface HDRCopierLayer : CAMetalLayer
- (id)init;
- (void)setHDRContents:(IOSurfaceRef)buffer
                device:(id<MTLDevice>)device
     screenHdrHeadroom:(float)screenHdrHeadroom
            colorSpace:(gfx::ColorSpace)colorSpace
              metadata:(std::optional<gfx::HDRMetadata>)hdrMetadata;
@end

@implementation HDRCopierLayer {
  id<MTLRenderPipelineState> __strong _renderPipelineState;
  gfx::ColorSpace _colorSpace;
  std::optional<gfx::HDRMetadata> _hdrMetadata;
}
- (id)init {
  if ((self = [super init])) {
    id<MTLDevice> device = metal::GetDefaultDevice();
    if (@available(iOS 16.0, *)) {
      self.wantsExtendedDynamicRangeContent = YES;
    }
    self.device = device;
    self.opaque = NO;
    self.presentsWithTransaction = YES;
    self.pixelFormat = MTLPixelFormatRGBA16Float;
    base::apple::ScopedCFTypeRef<CGColorSpaceRef> colorSpace(
        CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearITUR_2020));
    self.colorspace = colorSpace.get();
  }
  return self;
}

- (void)setHDRContents:(IOSurfaceRef)buffer
                device:(id<MTLDevice>)device
     screenHdrHeadroom:(float)screenHdrHeadroom
            colorSpace:(gfx::ColorSpace)colorSpace
              metadata:(std::optional<gfx::HDRMetadata>)hdrMetadata {
  // Retrieve information about the IOSurface.
  size_t width = IOSurfaceGetWidth(buffer);
  size_t height = IOSurfaceGetHeight(buffer);
  uint32_t numPlanes = 1;
  MTLPixelFormat mtlFormat[2] = {MTLPixelFormatInvalid, MTLPixelFormatInvalid};
  bool isUnorm = false;
  if (!IOSurfaceGetMTLPixelFormat(buffer, numPlanes, mtlFormat, isUnorm)) {
    DLOG(ERROR) << "Unsupported IOSurface format.";
    return;
  }

  // Set metadata for tone mapping.
  if (@available(iOS 16.0, *)) {
    if (_colorSpace != colorSpace || _hdrMetadata != hdrMetadata) {
      CAEDRMetadata* edrMetadata = nil;
      switch (colorSpace.GetTransferID()) {
        case gfx::ColorSpace::TransferID::PQ: {
          base::apple::ScopedCFTypeRef<CFDataRef> display_info =
              gfx::GenerateMasteringDisplayColorVolume(hdrMetadata);
          base::apple::ScopedCFTypeRef<CFDataRef> content_info =
              gfx::GenerateContentLightLevelInfo(hdrMetadata);
          edrMetadata = [CAEDRMetadata
              HDR10MetadataWithDisplayInfo:base::apple::CFToNSPtrCast(
                                               display_info.get())
                               contentInfo:base::apple::CFToNSPtrCast(
                                               content_info.get())
                        opticalOutputScale:203];
          break;
        }
        case gfx::ColorSpace::TransferID::HLG:
          // HLG does not use the OS-provided tone mapping.
          // https://crbug.com/343249142
          break;
        default:
          break;
      }
      self.EDRMetadata = edrMetadata;
      _colorSpace = colorSpace;
      _hdrMetadata = hdrMetadata;
    }
  }
  // Migrate to the MTLDevice on which the CAMetalLayer is being composited, if
  // known.
  if (device) {
    self.device = device;
  } else {
    id<MTLDevice> preferredDevice = self.preferredDevice;
    if (preferredDevice) {
      self.device = preferredDevice;
    }
    device = self.device;
  }

  // When the device changes, rebuild the RenderPipelineState.
  if (device != _renderPipelineState.device) {
    _renderPipelineState = CreateRenderPipelineState(device);
  }
  if (!_renderPipelineState) {
    return;
  }

  // Update the layer's properties to match the IOSurface.
  self.drawableSize = CGSizeMake(width, height);

  // Create a texture to wrap the IOSurface.
  id<MTLTexture> bufferTexture[2] = {nil, nil};
  for (uint32_t i = 0; i < numPlanes; ++i) {
    MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
    texDesc.textureType = MTLTextureType2D;
    texDesc.usage = MTLTextureUsageShaderRead;
    texDesc.pixelFormat = mtlFormat[i];
    texDesc.width = IOSurfaceGetWidthOfPlane(buffer, i);
    texDesc.height = IOSurfaceGetHeightOfPlane(buffer, i);
    texDesc.depth = 1;
    texDesc.mipmapLevelCount = 1;
    texDesc.arrayLength = 1;
    texDesc.sampleCount = 1;
#if BUILDFLAG(IS_MAC)
    texDesc.storageMode = MTLStorageModeManaged;
#endif
    bufferTexture[i] = [device newTextureWithDescriptor:texDesc
                                              iosurface:buffer
                                                  plane:i];
  }

  // Create a texture to wrap the drawable.
  id<CAMetalDrawable> drawable = [self nextDrawable];
  id<MTLTexture> drawableTexture = drawable.texture;

  // Copy from the IOSurface to the drawable.
  id<MTLCommandQueue> commandQueue = [device newCommandQueue];
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
    [encoder setFragmentTexture:bufferTexture[0] atIndex:0];
    [encoder setFragmentTexture:bufferTexture[1] atIndex:1];
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

    // Matrix
    simd::float4x4 yuvToRgb;
    {
      SkM44 skYuvToRgb;
      if (!colorSpace.GetTransferMatrix(10).invert(&skYuvToRgb)) {
        return;
      }
      SkM44 m = skYuvToRgb * colorSpace.GetRangeAdjustMatrix(10);
      yuvToRgb = simd::float4x4(
          simd::make_float4(m.rc(0, 0), m.rc(1, 0), m.rc(2, 0), m.rc(3, 0)),
          simd::make_float4(m.rc(0, 1), m.rc(1, 1), m.rc(2, 1), m.rc(3, 1)),
          simd::make_float4(m.rc(0, 2), m.rc(1, 2), m.rc(2, 2), m.rc(3, 2)),
          simd::make_float4(m.rc(0, 3), m.rc(1, 3), m.rc(2, 3), m.rc(3, 3)));
    }

    // Compute the primary transform matrix from |color_space| to Rec2020.
    simd::float3x3 primaryMatrix;
    {
      skcms_Matrix3x3 src_to_xyz;
      skcms_Matrix3x3 rec2020_to_xyz;
      skcms_Matrix3x3 xyz_to_rec2020;
      SkNamedPrimariesExt::kRec2020.toXYZD50(&rec2020_to_xyz);
      colorSpace.GetPrimaryMatrix(&src_to_xyz);
      skcms_Matrix3x3_invert(&rec2020_to_xyz, &xyz_to_rec2020);
      skcms_Matrix3x3 m = skcms_Matrix3x3_concat(&xyz_to_rec2020, &src_to_xyz);
      primaryMatrix = simd::float3x3(
          simd::make_float3(m.vals[0][0], m.vals[1][0], m.vals[2][0]),
          simd::make_float3(m.vals[0][1], m.vals[1][1], m.vals[2][1]),
          simd::make_float3(m.vals[0][2], m.vals[1][2], m.vals[2][2]));
    }

    [encoder setVertexBytes:positions length:sizeof(positions) atIndex:0];
    [encoder setFragmentBytes:&yuvToRgb length:sizeof(yuvToRgb) atIndex:0];
    [encoder setFragmentBytes:&primaryMatrix
                       length:sizeof(primaryMatrix)
                      atIndex:1];
    [encoder setFragmentBytes:&numPlanes length:sizeof(numPlanes) atIndex:2];
    [encoder setFragmentBytes:&transferFunctionIndex
                       length:sizeof(transferFunctionIndex)
                      atIndex:3];
    [encoder setFragmentBytes:&fn length:sizeof(fn) atIndex:4];
    [encoder setFragmentBytes:&screenHdrHeadroom
                       length:sizeof(screenHdrHeadroom)
                      atIndex:5];
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

CALayer* MakeHDRCopierLayer() {
  return [[HDRCopierLayer alloc] init];
}

void UpdateHDRCopierLayer(CALayer* layer,
                          IOSurfaceRef buffer,
                          id<MTLDevice> device,
                          float screen_hdr_headroom,
                          const gfx::ColorSpace& color_space,
                          const std::optional<gfx::HDRMetadata>& hdr_metadata) {
  if (auto* hdr_copier_layer = base::apple::ObjCCast<HDRCopierLayer>(layer)) {
    [hdr_copier_layer setHDRContents:buffer
                              device:device
                   screenHdrHeadroom:screen_hdr_headroom
                          colorSpace:color_space
                            metadata:hdr_metadata];
    return;
  }
}

bool ShouldUseHDRCopier(IOSurfaceRef buffer,
                        const gfx::HDRMetadata& hdr_metadata,
                        const gfx::ColorSpace& color_space) {
  // Only some transfer functions are supported.
  if (!GetTransferFunctionIndex(color_space)) {
    return false;
  }

  // Only some pixel formats are supported.
  bool is_unorm = false;
  uint32_t num_planes = 0;
  MTLPixelFormat format[2] = {MTLPixelFormatInvalid, MTLPixelFormatInvalid};
  if (!IOSurfaceGetMTLPixelFormat(buffer, num_planes, format, is_unorm)) {
    return false;
  }

  // If this is a video frame (is multi-planar), then only override the default
  // behavior for HLG content.
  if (num_planes == 2) {
    if (color_space.GetTransferID() != gfx::ColorSpace::TransferID::HLG) {
      return false;
    }
    if (!base::FeatureList::IsEnabled(kMacHlgUseHdrCopier)) {
      return false;
    }
  }

  if (color_space.IsToneMappedByDefault()) {
    return true;
  }

  if (hdr_metadata.extended_range.has_value()) {
    return true;
  }

  // Rasterized tiles and the primary plane specify a color space of SRGB_HDR
  // with no extended range metadata.
  // TODO(crbug.com/40268540): Use extended range metadata instead of
  // the SDR_HDR color space to indicate this.
  if (color_space.GetTransferID() == gfx::ColorSpace::TransferID::SRGB_HDR) {
    return !is_unorm;
  }

  return false;
}

}  // namespace metal
