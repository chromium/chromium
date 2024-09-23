// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element.h"

#include <algorithm>
#include <limits>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "device/vr/vr_gl_util.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

namespace {

int AllocateId() {
  static int g_next_id = 1;
  return g_next_id++;
}

#ifndef NDEBUG
constexpr char kRed[] = "\x1b[31m";
constexpr char kGreen[] = "\x1b[32m";
constexpr char kBlue[] = "\x1b[34m";
constexpr char kCyan[] = "\x1b[36m";
constexpr char kYellow[] = "\x1b[33m";
constexpr char kReset[] = "\x1b[0m";

void DumpTransformOperations(const gfx::TransformOperations& ops,
                             std::ostringstream* os) {
  if (!ops.at(0).IsIdentity()) {
    const auto& translate = ops.at(0).translate;
    *os << "t(" << translate.x << ", " << translate.y << ", " << translate.z
        << ") ";
  }

  if (ops.size() < 2u) {
    return;
  }

  if (!ops.at(1).IsIdentity()) {
    const auto& rotate = ops.at(1).rotate;
    if (rotate.axis.x > 0.0f) {
      *os << "rx(" << rotate.angle << ") ";
    } else if (rotate.axis.y > 0.0f) {
      *os << "ry(" << rotate.angle << ") ";
    } else if (rotate.axis.z > 0.0f) {
      *os << "rz(" << rotate.angle << ") ";
    }
  }

  if (!ops.at(2).IsIdentity()) {
    const auto& scale = ops.at(2).scale;
    *os << "s(" << scale.x << ", " << scale.y << ", " << scale.z << ") ";
  }
}
#endif

}  // namespace

UiElement::UiElement() : id_(AllocateId()) {
  layout_offset_.AppendTranslate(0, 0, 0);
  transform_operations_.AppendTranslate(0, 0, 0);
  transform_operations_.AppendRotate(1, 0, 0, 0);
  transform_operations_.AppendScale(1, 1, 1);
}

UiElement::~UiElement() = default;

void UiElement::SetName(UiElementName name) {
  name_ = name;
}

void UiElement::SetType(UiElementType type) {
  type_ = type;
}

void UiElement::SetDrawPhase(DrawPhase draw_phase) {
  draw_phase_ = draw_phase;
}

void UiElement::Render(UiElementRenderer* renderer,
                       const CameraModel& model) const {
  // Elements without an overridden implementation of Render should have their
  // draw phase set to kPhaseNone and should, consequently, be filtered out when
  // the UiRenderer collects elements to draw. Therefore, if we invoke this
  // function, it is an error.
  NOTREACHED_IN_MIGRATION() << "element: " << DebugName();
}

void UiElement::Initialize(SkiaSurfaceProvider* provider) {}

bool UiElement::DoBeginFrame(const gfx::Transform& head_pose,
                             bool force_animations_to_completion) {
  // TODO(mthiesse): This is overly cautious. We may have keyframe_models but
  // not trigger any updates, so we should refine this logic and have
  // KeyframeEffect::Tick return a boolean. Similarly, the bindings update
  // may have had no visual effect and dirtiness should be related to setting
  // properties that do indeed cause visual updates.
  bool keyframe_models_updated = !animator_.keyframe_models().empty();
  if (force_animations_to_completion) {
    animator_.FinishAll();
  } else {
    animator_.Tick(last_frame_time_);
  }
  set_update_phase(kUpdatedAnimations);
  bool begin_frame_updated = OnBeginFrame(head_pose);
  UpdateComputedOpacity();
  bool was_visible_at_any_point = IsVisible() ||
                                  updated_visibility_this_frame_ ||
                                  IsOrWillBeLocallyVisible();
  bool dirty = (begin_frame_updated || keyframe_models_updated ||
                updated_bindings_this_frame_) &&
               was_visible_at_any_point;

  if (was_visible_at_any_point) {
    for (auto& child : children_)
      dirty |= child->DoBeginFrame(head_pose, force_animations_to_completion);
  }

  return dirty;
}

bool UiElement::OnBeginFrame(const gfx::Transform& head_pose) {
  return false;
}

bool UiElement::PrepareToDraw() {
  return false;
}

bool UiElement::HasDirtyTexture() const {
  return false;
}

void UiElement::UpdateTexture() {}

void UiElement::SetSize(float width, float height) {
  animator_.TransitionSizeTo(this, last_frame_time_, BOUNDS, size_,
                             gfx::SizeF(width, height));
}

void UiElement::SetVisible(bool visible) {
  SetOpacity(visible ? opacity_when_visible_ : 0.0);
}

void UiElement::SetVisibleImmediately(bool visible) {
  opacity_ = visible ? opacity_when_visible_ : 0.0;
  animator_.RemoveKeyframeModels(OPACITY);
}

bool UiElement::IsVisible() const {
  // Many things rely on checking element visibility, including tests.
  // Therefore, support reporting visibility even if an element sits in an
  // invisible portion of the tree.  We can infer that if the scene computed
  // opacities, but this element did not, it must be invisible.
  DCHECK(update_phase_ >= kUpdatedComputedOpacity ||
         FrameLifecycle::phase() >= kUpdatedComputedOpacity);
  // TODO(crbug.com/41382805): we shouldn't need to check opacity() here.
  return update_phase_ >= kUpdatedComputedOpacity && opacity() > 0.0f &&
         computed_opacity() > 0.0f;
}

bool UiElement::IsOrWillBeLocallyVisible() const {
  return opacity() > 0.0f || GetTargetOpacity() > 0.0f;
}

gfx::SizeF UiElement::size() const {
  DCHECK_LE(kUpdatedSize, update_phase_);
  return size_;
}

void UiElement::SetLayoutOffset(float x, float y) {
  if (x_centering() == LEFT) {
    x += size_.width() / 2;
    if (!bounds_contain_padding_)
      x -= left_padding_;
  } else if (x_centering() == RIGHT) {
    x -= size_.width() / 2;
    if (!bounds_contain_padding_)
      x += right_padding_;
  }
  if (y_centering() == TOP) {
    y -= size_.height() / 2;
    if (!bounds_contain_padding_)
      y += top_padding_;
  } else if (y_centering() == BOTTOM) {
    y += size_.height() / 2;
    if (!bounds_contain_padding_)
      y -= bottom_padding_;
  }

  if (x == layout_offset_.at(0).translate.x &&
      y == layout_offset_.at(0).translate.y &&
      !IsAnimatingProperty(LAYOUT_OFFSET)) {
    return;
  }

  gfx::TransformOperations operations = layout_offset_;
  gfx::TransformOperation& op = operations.at(0);
  op.translate = {x, y, 0};
  op.Bake();
  animator_.TransitionTransformOperationsTo(
      this, last_frame_time_, LAYOUT_OFFSET, layout_offset_, operations);
}

void UiElement::SetTranslate(float x, float y, float z) {
  if (x == transform_operations_.at(kTranslateIndex).translate.x &&
      y == transform_operations_.at(kTranslateIndex).translate.y &&
      z == transform_operations_.at(kTranslateIndex).translate.z &&
      !IsAnimatingProperty(TRANSFORM)) {
    return;
  }

  gfx::TransformOperations operations = transform_operations_;
  gfx::TransformOperation& op = operations.at(kTranslateIndex);
  op.translate = {x, y, z};
  op.Bake();
  animator_.TransitionTransformOperationsTo(this, last_frame_time_, TRANSFORM,
                                            transform_operations_, operations);
}

void UiElement::SetRotate(float x, float y, float z, float radians) {
  float degrees = base::RadToDeg(radians);

  if (x == transform_operations_.at(kRotateIndex).rotate.axis.x &&
      y == transform_operations_.at(kRotateIndex).rotate.axis.y &&
      z == transform_operations_.at(kRotateIndex).rotate.axis.z &&
      degrees == transform_operations_.at(kRotateIndex).rotate.angle &&
      !IsAnimatingProperty(TRANSFORM)) {
    return;
  }

  gfx::TransformOperations operations = transform_operations_;
  gfx::TransformOperation& op = operations.at(kRotateIndex);
  op.rotate.axis = {x, y, z};
  op.rotate.angle = degrees;
  op.Bake();
  animator_.TransitionTransformOperationsTo(this, last_frame_time_, TRANSFORM,
                                            transform_operations_, operations);
}

void UiElement::SetScale(float x, float y, float z) {
  if (x == transform_operations_.at(kScaleIndex).scale.x &&
      y == transform_operations_.at(kScaleIndex).scale.y &&
      z == transform_operations_.at(kScaleIndex).scale.z &&
      !IsAnimatingProperty(TRANSFORM)) {
    return;
  }

  gfx::TransformOperations operations = transform_operations_;
  gfx::TransformOperation& op = operations.at(kScaleIndex);
  op.scale = {x, y, z};
  op.Bake();
  animator_.TransitionTransformOperationsTo(this, last_frame_time_, TRANSFORM,
                                            transform_operations_, operations);
}

void UiElement::SetOpacity(float opacity) {
  animator_.TransitionFloatTo(this, last_frame_time_, OPACITY, opacity_,
                              opacity);
}

void UiElement::SetCornerRadii(const CornerRadii& radii) {
  corner_radii_ = radii;
}

gfx::SizeF UiElement::GetTargetSize() const {
  return animator_.GetTargetSizeValue(TargetProperty::BOUNDS, size_);
}

gfx::TransformOperations UiElement::GetTargetTransform() const {
  return animator_.GetTargetTransformOperationsValue(TargetProperty::TRANSFORM,
                                                     transform_operations_);
}

gfx::Transform UiElement::ComputeTargetWorldSpaceTransform() const {
  gfx::Transform m;
  for (const UiElement* current = this; current; current = current->parent()) {
    m.PostConcat(current->GetTargetLocalTransform());
  }
  return m;
}

float UiElement::GetTargetOpacity() const {
  return animator_.GetTargetFloatValue(TargetProperty::OPACITY, opacity_);
}

float UiElement::ComputeTargetOpacity() const {
  float opacity = 1.0;
  for (const UiElement* current = this; current; current = current->parent()) {
    opacity *= current->GetTargetOpacity();
  }
  return opacity;
}

float UiElement::computed_opacity() const {
  DCHECK_LE(kUpdatedComputedOpacity, update_phase_) << DebugName();
  return computed_opacity_;
}

const gfx::Transform& UiElement::world_space_transform() const {
  DCHECK_LE(kUpdatedWorldSpaceTransform, update_phase_);
  return world_space_transform_;
}

bool UiElement::IsWorldPositioned() const {
  return true;
}

std::string UiElement::DebugName() const {
  return base::StringPrintf(
      "%s%s%s",
      UiElementNameToString(name() == kNone ? owner_name_for_test() : name())
          .c_str(),
      type() == kTypeNone ? "" : ":",
      type() == kTypeNone ? "" : UiElementTypeToString(type()).c_str());
}

#ifndef NDEBUG
void DumpLines(const std::vector<size_t>& counts,
               const std::vector<const UiElement*>& ancestors,
               std::ostringstream* os) {
  for (size_t i = 0; i < counts.size(); ++i) {
    size_t current_count = counts[i];
    if (i + 1 < counts.size()) {
      current_count++;
    }
    if (ancestors[ancestors.size() - i - 1]->children().size() >
        current_count) {
      *os << "| ";
    } else {
      *os << "  ";
    }
  }
}

void UiElement::DumpHierarchy(std::vector<size_t> counts,
                              std::ostringstream* os,
                              bool include_bindings) const {
  // Put our ancestors in a vector for easy reverse traversal.
  std::vector<const UiElement*> ancestors;
  for (const UiElement* ancestor = parent(); ancestor;
       ancestor = ancestor->parent()) {
    ancestors.push_back(ancestor);
  }
  DCHECK_EQ(counts.size(), ancestors.size());

  *os << kBlue;
  for (size_t i = 0; i < counts.size(); ++i) {
    if (i + 1 == counts.size()) {
      *os << "+-";
    } else if (ancestors[ancestors.size() - i - 1]->children().size() >
               counts[i] + 1) {
      *os << "| ";
    } else {
      *os << "  ";
    }
  }
  *os << kReset;

  if (update_phase_ < kUpdatedComputedOpacity || !IsVisible()) {
    *os << kBlue;
  }

  *os << DebugName() << kReset << " " << kCyan << DrawPhaseToString(draw_phase_)
      << " " << kReset;

  if (update_phase_ >= kUpdatedSize) {
    if (size().width() != 0.0f || size().height() != 0.0f) {
      *os << kRed << "[" << size().width() << ", " << size().height() << "] "
          << kReset;
    }
  }

  if (update_phase_ >= kUpdatedWorldSpaceTransform) {
    *os << kGreen;
    DumpGeometry(os);
  }

  counts.push_back(0u);

  if (include_bindings) {
    std::ostringstream binding_stream;
    for (auto& binding : bindings_) {
      std::string binding_text = binding->ToString();
      if (binding_text.empty())
        continue;
      binding_stream << binding->ToString() << std::endl;
    }

    auto split_bindings =
        base::SplitString(binding_stream.str(), "\n", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (!split_bindings.empty()) {
      ancestors.insert(ancestors.begin(), this);
    }
    for (const auto& split : split_bindings) {
      *os << std::endl << kBlue;
      DumpLines(counts, ancestors, os);
      *os << kGreen << split;
    }
  }

  *os << kReset << std::endl;

  for (auto& child : children_) {
    child->DumpHierarchy(counts, os, include_bindings);
    counts.back()++;
  }
}

void UiElement::DumpGeometry(std::ostringstream* os) const {
  DumpTransformOperations(transform_operations_, os);
  *os << kYellow;
  DumpTransformOperations(layout_offset_, os);
}
#endif

void UiElement::AddChild(std::unique_ptr<UiElement> child) {
  for (UiElement* current = this; current; current = current->parent())
    current->set_descendants_updated(true);
  child->parent_ = this;
  children_.push_back(std::move(child));
}

std::unique_ptr<UiElement> UiElement::RemoveChild(UiElement* to_remove) {
  return ReplaceChild(to_remove, nullptr);
}

std::unique_ptr<UiElement> UiElement::ReplaceChild(
    UiElement* to_remove,
    std::unique_ptr<UiElement> to_add) {
  for (UiElement* current = this; current; current = current->parent())
    current->set_descendants_updated(true);
  DCHECK_EQ(this, to_remove->parent_);
  to_remove->parent_ = nullptr;
  size_t old_size = children_.size();

  auto it = base::ranges::find(children_, to_remove,
                               &std::unique_ptr<UiElement>::get);
  DCHECK(it != std::end(children_));

  std::unique_ptr<UiElement> removed(it->release());
  if (to_add) {
    to_add->parent_ = this;
    *it = std::move(to_add);
  } else {
    children_.erase(it);
    DCHECK_EQ(old_size - 1, children_.size());
  }
  return removed;
}

void UiElement::AddBinding(std::unique_ptr<BindingBase> binding) {
  bindings_.push_back(std::move(binding));
}

void UiElement::UpdateBindings() {
  bool should_recur = IsOrWillBeLocallyVisible();
  updated_bindings_this_frame_ = false;
  for (auto& binding : bindings_) {
    if (binding->Update())
      updated_bindings_this_frame_ = true;
  }
  should_recur |= IsOrWillBeLocallyVisible();

  set_update_phase(kUpdatedBindings);
  if (!should_recur)
    return;

  for (auto& child : children_)
    child->UpdateBindings();
}

gfx::Point3F UiElement::GetCenter() const {
  return world_space_transform_.MapPoint(gfx::Point3F());
}

void UiElement::OnFloatAnimated(const float& value,
                                int target_property_id,
                                gfx::KeyframeModel* keyframe_model) {
  opacity_ = std::clamp(value, 0.0f, 1.0f);
}

void UiElement::OnTransformAnimated(const gfx::TransformOperations& operations,
                                    int target_property_id,
                                    gfx::KeyframeModel* keyframe_model) {
  if (target_property_id == TRANSFORM) {
    transform_operations_ = operations;
  } else if (target_property_id == LAYOUT_OFFSET) {
    layout_offset_ = operations;
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  local_transform_ = layout_offset_.Apply() * transform_operations_.Apply();
  world_space_transform_dirty_ = true;
}

void UiElement::OnSizeAnimated(const gfx::SizeF& size,
                               int target_property_id,
                               gfx::KeyframeModel* keyframe_model) {
  if (size_ == size)
    return;
  size_ = size;
  world_space_transform_dirty_ = true;
}

void UiElement::OnColorAnimated(const SkColor& size,
                                int target_property_id,
                                gfx::KeyframeModel* keyframe_model) {}

void UiElement::SetTransitionedProperties(
    const std::set<TargetProperty>& properties) {
  std::set<int> converted_properties(properties.begin(), properties.end());
  animator_.SetTransitionedProperties(converted_properties);
}

void UiElement::AddKeyframeModel(
    std::unique_ptr<gfx::KeyframeModel> keyframe_model) {
  animator_.AddKeyframeModel(std::move(keyframe_model));
}

void UiElement::RemoveKeyframeModels(int target_property) {
  animator_.RemoveKeyframeModels(target_property);
}

bool UiElement::IsAnimatingProperty(TargetProperty property) const {
  return animator_.IsAnimatingProperty(static_cast<int>(property));
}

bool UiElement::SizeAndLayOut() {
  if (!IsVisible() && !IsOrWillBeLocallyVisible())
    return false;

  // May be overridden by layout elements.
  bool changed = SizeAndLayOutChildren();

  changed |= PrepareToDraw();

  LayOutContributingChildren();

  if (bounds_contain_children_) {
    gfx::RectF bounds = ComputeContributingChildrenBounds();
    if (bounds.size() != GetTargetSize())
      SetSize(bounds.width(), bounds.height());
  } else {
    DCHECK_EQ(0.0f, right_padding_);
    DCHECK_EQ(0.0f, left_padding_);
    DCHECK_EQ(0.0f, top_padding_);
    DCHECK_EQ(0.0f, bottom_padding_);
  }
  set_update_phase(kUpdatedSize);

  LayOutNonContributingChildren();

  set_update_phase(kUpdatedLayout);
  return changed;
}

bool UiElement::SizeAndLayOutChildren() {
  bool changed = false;
  for (auto& child : children_)
    changed |= child->SizeAndLayOut();
  return changed;
}

gfx::RectF UiElement::ComputeContributingChildrenBounds() {
  gfx::RectF bounds;
  for (auto& child : children_) {
    if (!child->IsVisible() || !child->contributes_to_parent_bounds())
      continue;

    gfx::RectF outer_bounds(child->size());
    gfx::RectF inner_bounds(child->size());
    if (!child->bounds_contain_padding_) {
      inner_bounds.Inset(
          gfx::InsetsF::TLBR(child->top_padding_, child->left_padding_,
                             child->bottom_padding_, child->right_padding_));
    }
    gfx::SizeF size = inner_bounds.size();
    if (size.IsEmpty())
      continue;

    gfx::Vector2dF delta =
        outer_bounds.CenterPoint() - inner_bounds.CenterPoint();
    gfx::Point3F child_center(child->local_origin() - delta);
    gfx::Vector3dF corner_offset(size.width(), size.height(), 0);
    corner_offset.Scale(-0.5);
    gfx::Point3F child_upper_left = child_center + corner_offset;
    gfx::Point3F child_lower_right = child_center - corner_offset;

    child_upper_left = child->LocalTransform().MapPoint(child_upper_left);
    child_lower_right = child->LocalTransform().MapPoint(child_lower_right);
    gfx::RectF local_rect =
        gfx::RectF(child_upper_left.x(), child_upper_left.y(),
                   child_lower_right.x() - child_upper_left.x(),
                   child_lower_right.y() - child_upper_left.y());
    bounds.Union(local_rect);
  }

  bounds.Inset(gfx::InsetsF::TLBR(-top_padding_, -left_padding_,
                                  -bottom_padding_, -right_padding_));
  bounds.set_origin(bounds.CenterPoint());
  if (local_origin_ != bounds.origin()) {
    world_space_transform_dirty_ = true;
    local_origin_ = bounds.origin();
  }

  return bounds;
}

void UiElement::LayOutContributingChildren() {
  for (auto& child : children_) {
    if (!child->IsVisible() || !child->contributes_to_parent_bounds())
      continue;
    // Nothing to actually do since we aren't a layout object.  Children that
    // contribute to parent bounds cannot center or anchor to the edge of the
    // parent.
    DCHECK_EQ(child->x_centering(), NONE) << child->DebugName();
    DCHECK_EQ(child->y_centering(), NONE) << child->DebugName();
    DCHECK_EQ(child->x_anchoring(), NONE) << child->DebugName();
    DCHECK_EQ(child->y_anchoring(), NONE) << child->DebugName();
  }
}

void UiElement::LayOutNonContributingChildren() {
  DCHECK_LE(kUpdatedSize, update_phase_);
  for (auto& child : children_) {
    if (!child->IsVisible() || child->contributes_to_parent_bounds())
      continue;

    // To anchor a child, use the parent's size to find its edge.
    float x_offset = 0.0f;
    if (child->x_anchoring() == LEFT) {
      x_offset = -0.5f * size().width();
      if (!child->bounds_contain_padding())
        x_offset += left_padding_;
    } else if (child->x_anchoring() == RIGHT) {
      x_offset = 0.5f * size().width();
      if (!child->bounds_contain_padding())
        x_offset -= right_padding_;
    }
    float y_offset = 0.0f;
    if (child->y_anchoring() == TOP) {
      y_offset = 0.5f * size().height();
      if (!child->bounds_contain_padding())
        y_offset -= top_padding_;
    } else if (child->y_anchoring() == BOTTOM) {
      y_offset = -0.5f * size().height();
      if (!child->bounds_contain_padding())
        y_offset += bottom_padding_;
    }
    child->SetLayoutOffset(x_offset, y_offset);
  }
}

gfx::RectF UiElement::GetClipRect() const {
  auto corner_origin = clip_rect_.origin() - gfx::Vector2dF(-0.5f, 0.5f);
  return gfx::RectF({corner_origin.x(), -corner_origin.y()}, clip_rect_.size());
}

void UiElement::SetClipRect(const gfx::RectF& rect) {
  auto new_origin = gfx::PointF(rect.origin().x(), -rect.origin().y()) +
                    gfx::Vector2dF(-0.5f, 0.5f);
  clip_rect_ = gfx::RectF(new_origin, rect.size());
}

void UiElement::UpdateComputedOpacity() {
  bool was_visible = computed_opacity_ > 0.0f;
  set_computed_opacity(opacity_);
  if (parent_) {
    set_computed_opacity(computed_opacity_ * parent_->computed_opacity());
  }
  set_update_phase(kUpdatedComputedOpacity);
  updated_visibility_this_frame_ = IsVisible() != was_visible;
}

bool UiElement::UpdateWorldSpaceTransform(bool parent_changed) {
  if (!IsVisible() && !updated_visibility_this_frame_)
    return false;

  bool changed = false;
  bool should_update = ShouldUpdateWorldSpaceTransform(parent_changed);
  if (should_update) {
    gfx::Transform transform;
    transform.Translate(local_origin_.x(), local_origin_.y());

    if (!size_.IsEmpty()) {
      transform.Scale(size_.width(), size_.height());
    }

    // Compute an inheritable transformation that can be applied to this
    // element, and it's children, if applicable.
    gfx::Transform inheritable = LocalTransform();

    if (parent_) {
      inheritable.PostConcat(parent_->inheritable_transform());
    }

    transform.PostConcat(inheritable);
    changed = !transform.ApproximatelyEqual(world_space_transform_) ||
              !inheritable.ApproximatelyEqual(inheritable_transform_);
    set_world_space_transform(transform);
    set_inheritable_transform(inheritable);
  }

  bool child_changed = false;
  set_update_phase(kUpdatedWorldSpaceTransform);
  for (auto& child : children_) {
    // TODO(crbug.com/41393128): it's unfortunate that we're not passing down
    // the same dirtiness signal that we return. I.e., we'd ideally use
    // |changed| here.
    child_changed |= child->UpdateWorldSpaceTransform(should_update);
  }

  return changed || child_changed;
}

gfx::Transform UiElement::LocalTransform() const {
  return local_transform_;
}

gfx::Transform UiElement::GetTargetLocalTransform() const {
  return layout_offset_.Apply() * GetTargetTransform().Apply();
}

bool UiElement::ShouldUpdateWorldSpaceTransform(
    bool parent_transform_changed) const {
  return parent_transform_changed || world_space_transform_dirty_;
}

}  // namespace vr
