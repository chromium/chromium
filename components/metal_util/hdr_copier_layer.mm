// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/hdr_copier_layer.h"

#include <CoreVideo/CVPixelBuffer.h>
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/metal_util/device.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"
#include "ui/gfx/color_space.h"

namespace {

// Source of the shader to perform tonemapping. Note that the functions
// ToLinearSRGB, ToLinearPQ, and ToLinearHLG are copy-pasted from the GLSL
// shader source in gfx::ColorTransform.
// TODO(https://crbug.com/1101041): Add non-identity tonemapping to the shader.
const char* tonemapping_shader_source =
    "#include <metal_stdlib>\n"
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
    "float ToLinearSRGB(float v) {\n"
    "  float abs_v = abs(v);\n"
    "  float sgn_v = sign(v);\n"
    "  if (abs_v < 0.0404482362771082f)\n"
    "    return v/12.92f;\n"
    "  else\n"
    "    return sgn_v*pow((abs_v+0.055f)/1.055f, 2.4f);\n"
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
    "                               constant uint32_t& f [[buffer(1)]]) {\n"
    "    constexpr sampler s(metal::mag_filter::nearest,\n"
    "                        metal::min_filter::nearest);\n"
    "    float4 color = t.sample(s, in.texcoord);\n"
    "    switch (f) {\n"
    "      case 1:\n"
    "         color.x = ToLinearSRGB(color.x);\n"
    "         color.y = ToLinearSRGB(color.y);\n"
    "         color.z = ToLinearSRGB(color.z);\n"
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
  switch (color_space.GetTransferID()) {
    case gfx::ColorSpace::TransferID::SRGB_HDR:
      return 1;
    case gfx::ColorSpace::TransferID::PQ:
      return 2;
    case gfx::ColorSpace::TransferID::HLG:
      return 3;
    default:
      return 0;
  }
}

// Convert from an IOSurface's pixel format to a MTLPixelFormat. Crash on any
// unsupported formats.
MTLPixelFormat IOSurfaceGetMTLPixelFormat(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.13)) {
  uint32_t format = IOSurfaceGetPixelFormat(buffer);
  switch (format) {
    case kCVPixelFormatType_64RGBAHalf:
      return MTLPixelFormatRGBA16Float;
    case kCVPixelFormatType_ARGB2101010LEPacked:
      return MTLPixelFormatBGR10A2Unorm;
    default:
      break;
  }
  return MTLPixelFormatInvalid;
}

base::scoped_nsprotocol<id<MTLRenderPipelineState>> CreateRenderPipelineState(
    id<MTLDevice> device) API_AVAILABLE(macos(10.13)) {
  base::scoped_nsprotocol<id<MTLRenderPipelineState>> render_pipeline_state;

  base::scoped_nsprotocol<id<MTLLibrary>> library;
  {
    NSError* error = nil;
    base::scoped_nsobject<NSString> source([[NSString alloc]
        initWithCString:tonemapping_shader_source
               encoding:NSASCIIStringEncoding]);
    base::scoped_nsobject<MTLCompileOptions> options(
        [[MTLCompileOptions alloc] init]);
    library.reset([device newLibraryWithSource:source
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

#if !defined(MAC_OS_X_VERSION_10_15)
API_AVAILABLE(macos(10.15))
@interface CAMetalLayer (Forward)
@property(readonly) id<MTLDevice> preferredDevice;
@end
#endif

API_AVAILABLE(macos(10.15))
@interface HDRCopierLayer : CAMetalLayer {
  base::scoped_nsprotocol<id<MTLRenderPipelineState>> _render_pipeline_state;
}
- (id)init;
- (void)setHDRContents:(IOSurfaceRef)buffer
        withColorSpace:(gfx::ColorSpace)color_space;
@end

@implementation HDRCopierLayer
- (id)init {
  if (self = [super init]) {
    base::scoped_nsprotocol<id<MTLDevice>> device(metal::CreateDefaultDevice());
    [self setWantsExtendedDynamicRangeContent:YES];
    [self setDevice:device];
    [self setOpaque:NO];
    [self setPresentsWithTransaction:YES];
    [self setPixelFormat:MTLPixelFormatRGBA16Float];
    [self setColorspace:CGColorSpaceCreateWithName(
                            kCGColorSpaceExtendedLinearSRGB)];
  }
  return self;
}

- (void)setHDRContents:(IOSurfaceRef)buffer
        withColorSpace:(gfx::ColorSpace)color_space {
  // Retrieve information about the IOSurface.
  size_t width = IOSurfaceGetWidth(buffer);
  size_t height = IOSurfaceGetHeight(buffer);
  MTLPixelFormat mtl_format = IOSurfaceGetMTLPixelFormat(buffer);
  if (mtl_format == MTLPixelFormatInvalid) {
    DLOG(ERROR) << "Unsupported IOSurface format.";
    return;
  }

  // Migrate to the MTLDevice on which the CAMetalLayer is being composited, if
  // known.
  if ([self respondsToSelector:@selector(preferredDevice)]) {
    id<MTLDevice> preferred_device = nil;
    if (preferred_device)
      [self setDevice:preferred_device];
  }
  id<MTLDevice> device = [self device];

  // When the device changes, rebuild the RenderPipelineState.
  if (device != [_render_pipeline_state device])
    _render_pipeline_state = CreateRenderPipelineState(device);
  if (!_render_pipeline_state)
    return;

  // Update the layer's properties to match the IOSurface.
  [self setDrawableSize:CGSizeMake(width, height)];

  // Create a texture to wrap the IOSurface.
  base::scoped_nsprotocol<id<MTLTexture>> buffer_texture;
  {
    base::scoped_nsobject<MTLTextureDescriptor> tex_desc(
        [MTLTextureDescriptor new]);
    [tex_desc setTextureType:MTLTextureType2D];
    [tex_desc setUsage:MTLTextureUsageShaderRead];
    [tex_desc setPixelFormat:mtl_format];
    [tex_desc setWidth:width];
    [tex_desc setHeight:height];
    [tex_desc setDepth:1];
    [tex_desc setMipmapLevelCount:1];
    [tex_desc setArrayLength:1];
    [tex_desc setSampleCount:1];
    [tex_desc setStorageMode:MTLStorageModeManaged];
    buffer_texture.reset([device newTextureWithDescriptor:tex_desc
                                                iosurface:buffer
                                                    plane:0]);
  }

  // Create a texture to wrap the drawable.
  id<CAMetalDrawable> drawable = [self nextDrawable];
  id<MTLTexture> drawable_texture = [drawable texture];

  // Copy from the IOSurface to the drawable.
  base::scoped_nsprotocol<id<MTLCommandQueue>> command_queue(
      [device newCommandQueue]);
  id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
  id<MTLRenderCommandEncoder> encoder = nil;
  {
    MTLRenderPassDescriptor* desc =
        [MTLRenderPassDescriptor renderPassDescriptor];
    desc.colorAttachments[0].texture = drawable_texture;
    desc.colorAttachments[0].loadAction = MTLLoadActionClear;
    desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
    encoder = [command_buffer renderCommandEncoderWithDescriptor:desc];

    MTLViewport viewport;
    viewport.originX = 0;
    viewport.originY = 0;
    viewport.width = width;
    viewport.height = height;
    viewport.znear = -1.0;
    viewport.zfar = 1.0;
    [encoder setViewport:viewport];
    [encoder setRenderPipelineState:_render_pipeline_state];
    [encoder setFragmentTexture:buffer_texture atIndex:0];
  }
  {
    simd::float2 positions[6] = {
        simd::make_float2(0, 0), simd::make_float2(0, 1),
        simd::make_float2(1, 1), simd::make_float2(1, 1),
        simd::make_float2(1, 0), simd::make_float2(0, 0),
    };

    // The value of |transfer_function| corresponds to the value as used in
    // the above shader source.
    uint32_t transfer_function_index = GetTransferFunctionIndex(color_space);
    DCHECK(transfer_function_index);

    // Matrix is the primary transform matrix from |color_space| to sRGB.
    simd::float3x3 matrix;
    {
      skcms_Matrix3x3 src_to_xyz;
      skcms_Matrix3x3 srgb_to_xyz;
      skcms_Matrix3x3 xyz_to_srgb;
      color_space.GetPrimaryMatrix(&src_to_xyz);
      gfx::ColorSpace::CreateSRGB().GetPrimaryMatrix(&srgb_to_xyz);
      skcms_Matrix3x3_invert(&srgb_to_xyz, &xyz_to_srgb);
      skcms_Matrix3x3 m = skcms_Matrix3x3_concat(&xyz_to_srgb, &src_to_xyz);
      matrix = simd::float3x3(
          simd::make_float3(m.vals[0][0], m.vals[1][0], m.vals[2][0]),
          simd::make_float3(m.vals[0][1], m.vals[1][1], m.vals[2][1]),
          simd::make_float3(m.vals[0][2], m.vals[1][2], m.vals[2][2]));
    }

    [encoder setFragmentBytes:&transfer_function_index
                       length:sizeof(transfer_function_index)
                      atIndex:1];
    [encoder setVertexBytes:positions length:sizeof(positions) atIndex:0];
    [encoder setFragmentBytes:&matrix length:sizeof(matrix) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                vertexStart:0
                vertexCount:6];
  }
  [encoder endEncoding];
  [command_buffer commit];
  [command_buffer waitUntilScheduled];
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

void UpdateHDRCopierLayer(CALayer* layer,
                          IOSurfaceRef buffer,
                          const gfx::ColorSpace& color_space) {
  if (@available(macos 10.15, *)) {
    if (auto* hdr_copier_layer = base::mac::ObjCCast<HDRCopierLayer>(layer)) {
      [hdr_copier_layer setHDRContents:buffer withColorSpace:color_space];
      return;
    }
  }
  NOTREACHED();
}

bool ShouldUseHDRCopier(IOSurfaceRef buffer,
                        const gfx::ColorSpace& color_space) {
  if (@available(macos 10.15, *)) {
    return GetTransferFunctionIndex(color_space) &&
           IOSurfaceGetMTLPixelFormat(buffer) != MTLPixelFormatInvalid;
  }
  return false;
}

}  // namespace metal
