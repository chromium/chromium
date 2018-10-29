// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/environment/background.h"

#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/skia_surface_provider.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace vr {

namespace {

// On the background sphere, this is the number of faces between poles.
int kSteps = 20;

// clang-format off
constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  uniform sampler2D u_NormalGradientTexture;
  uniform sampler2D u_IncognitoGradientTexture;
  uniform sampler2D u_FullscreenGradientTexture;
  uniform float u_NormalFactor;
  uniform float u_IncognitoFactor;
  uniform float u_FullscreenFactor;
  attribute vec2 a_TexCoordinate;
  varying vec2 v_TexCoordinate;
  varying vec4 v_GradientColor;

  void main() {
    vec4 normal_gradient_color =
        texture2D(u_NormalGradientTexture, a_TexCoordinate);
    vec4 incognito_gradient_color =
        texture2D(u_IncognitoGradientTexture, a_TexCoordinate);
    vec4 fullscreen_gradient_color =
        texture2D(u_FullscreenGradientTexture, a_TexCoordinate);

    v_GradientColor =
        normal_gradient_color * u_NormalFactor +
        incognito_gradient_color * u_IncognitoFactor +
        fullscreen_gradient_color * u_FullscreenFactor;

    vec4 sphereVertex;
    // The x coordinate maps linearly to yaw, so we just need to scale the input
    // range 0..1 to 0..2*pi.
    float theta = a_TexCoordinate.x * 6.28319;

    // The projection we use maps the y axis of the source image to sphere
    // altitude, not the pitch. If it were pitch, we would scale the y
    // coordinates from 0..1 to 0..pi, where 0 corresponds to the head pointing
    // straight down and pi is the head pointing straight up. But, if our source
    // is altitude, then we've roughly been given the cosine of the pitch - we
    // just need to remap the range 0..1 to 1..-1. And because the sine of the
    // pitch angle is always positive, we can easily compute it using the
    // sin^2(x) + cos^2(x) = 1 identity without worrying about sign.
    float cos_phi = 1.0 - 2.0 * a_TexCoordinate.y;
    float sin_phi = sqrt(1.0 - cos_phi * cos_phi);

    // Place the background at 1000 m, an arbitrary large distance. This
    // nullifies the translational portion of eye transforms, which is what we
    // want for ODS backgrounds.
    sphereVertex.x = 1000. * -cos(theta) * sin_phi;
    sphereVertex.y = 1000. * cos_phi;
    sphereVertex.z = 1000. * -sin(theta) * sin_phi;
    sphereVertex.w = 1.0;

    gl_Position = u_ModelViewProjMatrix * sphereVertex;
    v_TexCoordinate = a_TexCoordinate;
  }
);

constexpr char const* kFragmentShader = SHADER(
  precision mediump float;
  uniform sampler2D u_Texture;
  varying vec2 v_TexCoordinate;
  varying vec4 v_GradientColor;

  float OverlayChannel(float a, float b) {
    if (a < 0.5) {
      return 2.0 * a * b;
    }
    return 1.0 - 2.0 * (1.0 - a) * (1.0 - b);
  }

  void main() {
    vec4 background_color = texture2D(u_Texture, v_TexCoordinate);

    vec4 color = vec4(0.0, 0.0, 0.0, 1.0);

    color.r = OverlayChannel(v_GradientColor.r, background_color.r);
    color.g = OverlayChannel(v_GradientColor.g, background_color.g);
    color.b = OverlayChannel(v_GradientColor.b, background_color.b);

    // Add some noise to prevent banding artifacts in the gradient.
    float n = fract(dot(v_TexCoordinate, vec2(12345.67, 4567.89)) * 100.0);
    n = (n - 0.5) / 255.0;

    gl_FragColor = vec4(color.rgb + n, 1);
  }
);
/* clang-format on */

// The passed bitmap data is thrown away after the texture is generated.
GLuint UploadImage(std::unique_ptr<SkBitmap> bitmap,
                   SkiaSurfaceProvider* provider,
                   sk_sp<SkSurface>* surface) {
  if (bitmap) {
    *surface = provider->MakeSurface({bitmap->width(), bitmap->height()});
  } else {
    *surface = provider->MakeSurface({1, 1});
  }

  // TODO(tiborg): proper error handling.
  DCHECK(surface->get());
  SkCanvas* canvas = (*surface)->getCanvas();
  if (bitmap) {
    canvas->drawBitmap(*bitmap, 0, 0);
  } else {
    // If we are missing a gradient image, blending with channels at .5 will
    // have no effect -- it will be as if there is no gradient image.
    canvas->clear(0xFF808080);
  }

  return provider->FlushSurface(surface->get(), 0);
}

// Remaps 0..1 to 0..1 such that there is a concentration of values around 0.5.
float RemapLatitude(float t) {
  t = 2.0f * t - 1.0f;
  t = t * t * t;
  return 0.5f * (t + 1.0f);
}

}  // namespace

Background::Background() {
  SetTransitionedProperties(
      {NORMAL_COLOR_FACTOR, INCOGNITO_COLOR_FACTOR, FULLSCREEN_COLOR_FACTOR});
  SetTransitionDuration(base::TimeDelta::FromMilliseconds(2500));
}

Background::~Background() = default;

void Background::Render(UiElementRenderer* renderer,
                        const CameraModel& model) const {
  if (texture_handle_ != 0) {
    renderer->DrawBackground(
        model.view_proj_matrix * world_space_transform(), texture_handle_,
        normal_gradient_texture_handle_, incognito_gradient_texture_handle_,
        fullscreen_gradient_texture_handle_, normal_factor_, incognito_factor_,
        fullscreen_factor_);
  }
}

void Background::Initialize(SkiaSurfaceProvider* provider) {
  provider_ = provider;
  if (initialization_bitmap_) {
    CreateBackgroundTexture();
  }

  CreateGradientTextures();
}

void Background::SetBackgroundImage(std::unique_ptr<SkBitmap> background) {
  // We take the objects off the model here so that we can destroy them after
  // uploading to the GPU (they're pretty big).
  initialization_bitmap_ = std::move(background);
  if (provider_)
    CreateBackgroundTexture();
}

void Background::SetGradientImages(
    std::unique_ptr<SkBitmap> normal_gradient,
    std::unique_ptr<SkBitmap> incognito_gradient,
    std::unique_ptr<SkBitmap> fullscreen_gradient) {
  initialization_normal_gradient_bitmap_ = std::move(normal_gradient);
  initialization_incognito_gradient_bitmap_ = std::move(incognito_gradient);
  initialization_fullscreen_gradient_bitmap_ = std::move(fullscreen_gradient);
  if (provider_)
    CreateGradientTextures();
}

void Background::SetNormalFactor(float factor) {
  animation().TransitionFloatTo(last_frame_time(), NORMAL_COLOR_FACTOR,
                                normal_factor_, factor);
}

void Background::SetIncognitoFactor(float factor) {
  animation().TransitionFloatTo(last_frame_time(), INCOGNITO_COLOR_FACTOR,
                                incognito_factor_, factor);
}

void Background::SetFullscreenFactor(float factor) {
  animation().TransitionFloatTo(last_frame_time(), FULLSCREEN_COLOR_FACTOR,
                                fullscreen_factor_, factor);
}

void Background::CreateBackgroundTexture() {
  DCHECK(provider_);
  DCHECK(initialization_bitmap_);
  texture_handle_ =
      UploadImage(std::move(initialization_bitmap_), provider_, &surface_);
}

void Background::CreateGradientTextures() {
  DCHECK(provider_);
  normal_gradient_texture_handle_ =
      UploadImage(std::move(initialization_normal_gradient_bitmap_), provider_,
                  &normal_gradient_surface_);
  incognito_gradient_texture_handle_ =
      UploadImage(std::move(initialization_incognito_gradient_bitmap_),
                  provider_, &incognito_gradient_surface_);
  fullscreen_gradient_texture_handle_ =
      UploadImage(std::move(initialization_fullscreen_gradient_bitmap_),
                  provider_, &fullscreen_gradient_surface_);
}

void Background::NotifyClientFloatAnimated(float value,
                                           int target_property_id,
                                           cc::KeyframeModel* keyframe_model) {
  switch (target_property_id) {
    case NORMAL_COLOR_FACTOR:
      normal_factor_ = value;
      break;
    case INCOGNITO_COLOR_FACTOR:
      incognito_factor_ = value;
      break;
    case FULLSCREEN_COLOR_FACTOR:
      fullscreen_factor_ = value;
      break;
    default:
      UiElement::NotifyClientFloatAnimated(value, target_property_id,
                                           keyframe_model);
  }
}

Background::Renderer::Renderer()
    : BaseRenderer(kVertexShader, kFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  position_handle_ = glGetAttribLocation(program_handle_, "a_TexCoordinate");
  tex_uniform_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
  normal_gradient_tex_uniform_handle_ =
      glGetUniformLocation(program_handle_, "u_NormalGradientTexture");
  incognito_gradient_tex_uniform_handle_ =
      glGetUniformLocation(program_handle_, "u_IncognitoGradientTexture");
  fullscreen_gradient_tex_uniform_handle_ =
      glGetUniformLocation(program_handle_, "u_FullscreenGradientTexture");
  normal_factor_handle_ =
      glGetUniformLocation(program_handle_, "u_NormalFactor");
  incognito_factor_handle_ =
      glGetUniformLocation(program_handle_, "u_IncognitoFactor");
  fullscreen_factor_handle_ =
      glGetUniformLocation(program_handle_, "u_FullscreenFactor");

  // Build the set of texture points representing the sphere.
  std::vector<float> vertices;
  for (int x = 0; x <= 2 * kSteps; x++) {
    for (int y = 0; y <= kSteps; y++) {
      vertices.push_back(static_cast<float>(x) / (kSteps * 2));
      vertices.push_back(RemapLatitude(static_cast<float>(y) / kSteps));
    }
  }
  std::vector<GLushort> indices;
  int y_stride = kSteps + 1;
  for (int x = 0; x < 2 * kSteps; x++) {
    for (int y = 0; y < kSteps; y++) {
      GLushort p0 = x * y_stride + y;
      GLushort p1 = x * y_stride + y + 1;
      GLushort p2 = (x + 1) * y_stride + y;
      GLushort p3 = (x + 1) * y_stride + y + 1;
      indices.push_back(p0);
      indices.push_back(p1);
      indices.push_back(p3);
      indices.push_back(p0);
      indices.push_back(p3);
      indices.push_back(p2);
    }
  }

  GLuint buffers[2];
  glGenBuffers(2, buffers);
  vertex_buffer_ = buffers[0];
  index_buffer_ = buffers[1];
  index_count_ = indices.size();

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLushort),
               indices.data(), GL_STATIC_DRAW);
}

Background::Renderer::~Renderer() = default;

void Background::Renderer::Draw(const gfx::Transform& view_proj_matrix,
                                int texture_data_handle,
                                int normal_gradient_texture_data_handle,
                                int incognito_gradient_texture_data_handle,
                                int fullscreen_gradient_texture_data_handle,
                                float normal_factor,
                                float incognito_factor,
                                float fullscreen_factor) {
  glDisable(GL_BLEND);
  glUseProgram(program_handle_);

  // Pass in model view project matrix.
  glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                     MatrixToGLArray(view_proj_matrix).data());

  // Set up vertex attributes.
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glVertexAttribPointer(position_handle_, 2, GL_FLOAT, GL_FALSE, 0,
                        VOID_OFFSET(0));
  glEnableVertexAttribArray(position_handle_);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_data_handle);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, normal_gradient_texture_data_handle);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, incognito_gradient_texture_data_handle);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, fullscreen_gradient_texture_data_handle);

  // Set up uniforms.
  glUniform1i(tex_uniform_handle_, 0);
  glUniform1i(normal_gradient_tex_uniform_handle_, 1);
  glUniform1i(incognito_gradient_tex_uniform_handle_, 2);
  glUniform1i(fullscreen_gradient_tex_uniform_handle_, 3);

  glUniform1f(normal_factor_handle_, normal_factor);
  glUniform1f(incognito_factor_handle_, incognito_factor);
  glUniform1f(fullscreen_factor_handle_, fullscreen_factor);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

}  // namespace vr
