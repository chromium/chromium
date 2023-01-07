// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/controller.h"

#include "base/numerics/math_constants.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "device/vr/vr_gl_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace vr {

namespace {

constexpr SkColor kColor = SK_ColorWHITE;
// List of number pairs that each represent a stop on the opacity gradient.
// First number is the opacity, second number is the stop position (both in the
// range [0,1]).
constexpr float kBodyAlphaStops[] = {
    0.1f, 0.0f, 0.2f, 0.49f, 1.0f, 0.5f,
};
constexpr float kTopAlphaStops[] = {
    0.1f, 0.0f, 0.2f, 0.95f, 1.0f, 1.0f,
};
constexpr size_t kBodyNumRings = 10;
constexpr size_t kTopNumRings = 20;
constexpr size_t kNumSectors = 10;
const gfx::Vector3dF kUpVector(0.0f, 1.0f, 0.0f);
constexpr float kUnitLength = 1.0f;
constexpr float kUnitRadius = kUnitLength / 2;
constexpr size_t kCylinderNumRings = 1;
constexpr size_t kSquareNumSectors = 1;

// clang-format off
static constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  attribute vec3 a_Position;
  attribute vec4 a_Color;
  varying vec4 v_Color;

  void main() {
    v_Color = a_Color;
    gl_Position = u_ModelViewProjMatrix * vec4(a_Position, 1.0);
  }
);

static constexpr char const* kFragmentShader = SHADER(
    precision mediump float;
  uniform float u_Opacity;
  varying vec4 v_Color;

  void main() {
    gl_FragColor = vec4(v_Color.rgb, 1.0) * v_Color.a * u_Opacity;
  }
);
// clang-format on

std::unique_ptr<gfx::FloatAnimationCurve> CreateAlphaCurve(
    const float* alpha_stops,
    size_t length) {
  auto alpha_curve = gfx::KeyframedFloatAnimationCurve::Create();
  for (size_t i = 0; i < length; i += 2) {
    alpha_curve->AddKeyframe(gfx::FloatKeyframe::Create(
        base::Seconds(alpha_stops[i + 1]), alpha_stops[i], nullptr));
  }
  return alpha_curve;
}

void AddVertex(const gfx::Point3F& local_vertex,
               const gfx::Transform& transform,
               std::vector<float>* vertices) {
  gfx::Point3F vertex = transform.MapPoint(local_vertex);
  vertices->push_back(vertex.x());
  vertices->push_back(vertex.y());
  vertices->push_back(vertex.z());
}

void AddColor(float alpha,
              const gfx::FloatAnimationCurve& alpha_curve,
              std::vector<float>* colors) {
  colors->push_back(SkColorGetR(kColor) / 255.0);
  colors->push_back(SkColorGetG(kColor) / 255.0);
  colors->push_back(SkColorGetB(kColor) / 255.0);
  colors->push_back(alpha_curve.GetValue(base::Seconds(alpha)));
}

void AddSphere(size_t num_rings,
               size_t num_sectors,
               float arc_rings,
               float arc_sectors,
               const gfx::Transform& transform,
               const gfx::FloatAnimationCurve& alpha_curve,
               std::vector<float>* vertices,
               std::vector<float>* colors,
               std::vector<GLushort>* indices) {
  size_t index_offset = vertices->size() / 3;
  float step_rings = arc_rings / num_rings;
  float step_sectors = arc_sectors / num_sectors;

  for (size_t ring = 0; ring < num_rings + 1; ring++) {
    for (size_t sector = 0; sector < num_sectors + 1; sector++) {
      gfx::Point3F vertex(
          std::sin(2.0 * base::kPiFloat * sector * step_sectors) *
              std::sin(base::kPiFloat * ring * step_rings) * kUnitRadius,
          std::cos(base::kPiFloat * ring * step_rings) * kUnitRadius,
          std::cos(2.0 * base::kPiFloat * sector * step_sectors) *
              std::sin(base::kPiFloat * ring * step_rings) * kUnitRadius);
      AddVertex(vertex, transform, vertices);

      gfx::Vector3dF normal(vertex.x(), vertex.y(), vertex.z());
      AddColor(gfx::AngleBetweenVectorsInDegrees(normal, kUpVector) / 180,
               alpha_curve, colors);
    }
  }

  for (size_t ring = 0; ring < num_rings; ring++) {
    size_t ring_offset = ring * (num_sectors + 1);
    for (size_t sector = 0; sector < num_sectors; sector++) {
      size_t offset = ring_offset + sector + index_offset;
      indices->push_back(offset);
      indices->push_back(offset + num_sectors + 1);
      indices->push_back(offset + num_sectors + 2);
      indices->push_back(offset);
      indices->push_back(offset + num_sectors + 2);
      indices->push_back(offset + 1);
    }
  }
}

void AddCylinder(size_t num_rings,
                 size_t num_sectors,
                 float arc,
                 const gfx::Transform& transform,
                 const gfx::FloatAnimationCurve& alpha_curve,
                 std::vector<float>* vertices,
                 std::vector<float>* colors,
                 std::vector<GLushort>* indices) {
  size_t index_offset = vertices->size() / 3;
  float step_rings = 1.0 / num_rings;
  float step_sectors = arc / num_sectors;

  for (size_t ring = 0; ring < num_rings + 1; ring++) {
    for (size_t sector = 0; sector < num_sectors + 1; sector++) {
      gfx::Point3F vertex(
          -kUnitLength / 2 + ring * step_rings * kUnitLength,
          std::sin(2.0 * base::kPiFloat * sector * step_sectors) * kUnitRadius,
          std::cos(2.0 * base::kPiFloat * sector * step_sectors) * kUnitRadius);
      AddVertex(vertex, transform, vertices);

      gfx::Vector3dF normal(0.0f, vertex.y(), vertex.z());
      AddColor(gfx::AngleBetweenVectorsInDegrees(normal, kUpVector) / 180,
               alpha_curve, colors);
    }
  }

  for (size_t ring = 0; ring < num_rings; ring++) {
    size_t ring_offset = ring * (num_sectors + 1);
    for (size_t sector = 0; sector < num_sectors; sector++) {
      size_t offset = ring_offset + sector + index_offset;
      indices->push_back(offset);
      indices->push_back(offset + num_sectors + 1);
      indices->push_back(offset + 1);
      indices->push_back(offset + 1);
      indices->push_back(offset + num_sectors + 1);
      indices->push_back(offset + num_sectors + 2);
    }
  }
}

void AddCircle(size_t num_rings,
               size_t num_sectors,
               float arc,
               const gfx::Transform& transform,
               const gfx::FloatAnimationCurve& alpha_curve,
               std::vector<float>* vertices,
               std::vector<float>* colors,
               std::vector<GLushort>* indices) {
  size_t index_offset = vertices->size() / 3;
  float step_rings = 1.0 / num_rings;
  float step_sectors = arc / num_sectors;

  for (size_t ring = 0; ring < num_rings + 1; ring++) {
    for (size_t sector = 0; sector < num_sectors + 1; sector++) {
      gfx::Point3F vertex(
          std::sin(2.0 * base::kPiFloat * sector * step_sectors) * ring *
              step_rings * kUnitRadius,
          0.0f,
          std::cos(2.0 * base::kPiFloat * sector * step_sectors) * ring *
              step_rings * kUnitRadius);
      AddVertex(vertex, transform, vertices);

      AddColor(ring * step_rings, alpha_curve, colors);
    }
  }

  for (size_t ring = 0; ring < num_rings; ring++) {
    size_t ring_offset = ring * (num_sectors + 1);
    for (size_t sector = 0; sector < num_sectors; sector++) {
      size_t offset = ring_offset + sector + index_offset;
      indices->push_back(offset);
      indices->push_back(offset + num_sectors + 1);
      indices->push_back(offset + num_sectors + 2);
      indices->push_back(offset);
      indices->push_back(offset + num_sectors + 2);
      indices->push_back(offset + 1);
    }
  }
}

void AddSquare(size_t num_rings,
               size_t num_sectors,
               const gfx::Transform& transform,
               const gfx::FloatAnimationCurve& alpha_curve,
               std::vector<float>* vertices,
               std::vector<float>* colors,
               std::vector<GLushort>* indices) {
  size_t index_offset = vertices->size() / 3;
  float step_rings = 1.0 / num_rings;
  float step_sectors = 1.0 / num_sectors;

  for (size_t ring = 0; ring < num_rings + 1; ring++) {
    for (size_t sector = 0; sector < num_sectors + 1; sector++) {
      gfx::Point3F vertex(
          -kUnitLength / 2 + ring * step_rings * kUnitLength, 0.0,
          -kUnitLength / 2 + sector * step_sectors * kUnitLength);
      AddVertex(vertex, transform, vertices);

      AddColor(std::abs(vertex.x() * kUnitLength * 2), alpha_curve, colors);
    }
  }

  for (size_t ring = 0; ring < num_rings; ring++) {
    size_t ring_offset = ring * (num_sectors + 1);
    for (size_t sector = 0; sector < num_sectors; sector++) {
      size_t offset = ring_offset + sector + index_offset;
      indices->push_back(offset);
      indices->push_back(offset + num_sectors + 2);
      indices->push_back(offset + num_sectors + 1);
      indices->push_back(offset);
      indices->push_back(offset + 1);
      indices->push_back(offset + num_sectors + 2);
    }
  }
}

}  // namespace

Controller::Controller() = default;

Controller::~Controller() = default;

void Controller::Render(UiElementRenderer* renderer,
                        const CameraModel& model) const {
  renderer->DrawController(computed_opacity(),
                           model.view_proj_matrix * world_space_transform());
}

gfx::Transform Controller::LocalTransform() const {
  return local_transform_;
}

gfx::Transform Controller::GetTargetLocalTransform() const {
  return local_transform_;
}

Controller::Renderer::Renderer()
    : BaseRenderer(kVertexShader, kFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  color_handle_ = glGetAttribLocation(program_handle_, "a_Color");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");

  auto body_alpha_curve =
      CreateAlphaCurve(kBodyAlphaStops, std::size(kBodyAlphaStops));
  auto top_alpha_curve =
      CreateAlphaCurve(kTopAlphaStops, std::size(kTopAlphaStops));

  gfx::Transform transform;
  transform.Translate3d(0.0, 0.0, (kControllerLength - kControllerWidth) / 2);
  transform.Scale3d(kControllerWidth, kControllerHeight * 2, kControllerWidth);
  transform.RotateAboutXAxis(180);
  transform.RotateAboutYAxis(90);
  AddSphere(kBodyNumRings, kNumSectors, 0.5f, 0.5f, transform,
            *body_alpha_curve, &vertices_, &colors_, &indices_);

  transform.MakeIdentity();
  transform.Translate3d(0.0, 0.0, -(kControllerLength - kControllerWidth) / 2);
  transform.Scale3d(kControllerWidth, kControllerHeight * 2, kControllerWidth);
  transform.RotateAboutXAxis(180);
  transform.RotateAboutYAxis(-90);
  AddSphere(kBodyNumRings, kNumSectors, 0.5f, 0.5f, transform,
            *body_alpha_curve, &vertices_, &colors_, &indices_);

  transform.MakeIdentity();
  transform.Scale3d(kControllerWidth, kControllerHeight * 2,
                    kControllerLength - kControllerWidth);
  transform.RotateAboutXAxis(180);
  transform.RotateAboutYAxis(90);
  AddCylinder(kCylinderNumRings, kBodyNumRings * 2, 0.5f, transform,
              *body_alpha_curve, &vertices_, &colors_, &indices_);

  transform.MakeIdentity();
  transform.Translate3d(0.0, 0.0, (kControllerLength - kControllerWidth) / 2);
  transform.Scale3d(kControllerWidth, 1.0, kControllerWidth);
  transform.RotateAboutYAxis(-90);
  AddCircle(kTopNumRings / 2, kNumSectors, 0.5, transform, *top_alpha_curve,
            &vertices_, &colors_, &indices_);

  transform.MakeIdentity();
  transform.Translate3d(0.0, 0.0, -(kControllerLength - kControllerWidth) / 2);
  transform.Scale3d(kControllerWidth, 1.0, kControllerWidth);
  transform.RotateAboutYAxis(90);
  AddCircle(kTopNumRings / 2, kNumSectors, 0.5, transform, *top_alpha_curve,
            &vertices_, &colors_, &indices_);

  transform.MakeIdentity();
  transform.Scale3d(kControllerWidth, 1.0,
                    kControllerLength - kControllerWidth);
  AddSquare(kTopNumRings, kSquareNumSectors, transform, *top_alpha_curve,
            &vertices_, &colors_, &indices_);

  glGenBuffers(1, &vertex_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(float),
               vertices_.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glGenBuffers(1, &color_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, color_buffer_);
  glBufferData(GL_ARRAY_BUFFER, colors_.size() * sizeof(float), colors_.data(),
               GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glGenBuffers(1, &index_buffer_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(GLushort),
               indices_.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

Controller::Renderer::~Renderer() = default;

void Controller::Renderer::Draw(float opacity,
                                const gfx::Transform& model_view_proj_matrix) {
  glUseProgram(program_handle_);

  glUniform1f(opacity_handle_, opacity);
  glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                     MatrixToGLArray(model_view_proj_matrix).data());

  glEnableVertexAttribArray(position_handle_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glVertexAttribPointer(position_handle_, 3, GL_FLOAT, false, 3 * sizeof(float),
                        VOID_OFFSET(0));

  glEnableVertexAttribArray(color_handle_);
  glBindBuffer(GL_ARRAY_BUFFER, color_buffer_);
  glVertexAttribPointer(color_handle_, 4, GL_FLOAT, false, 4 * sizeof(float),
                        VOID_OFFSET(0));

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_SHORT, 0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

}  // namespace vr
