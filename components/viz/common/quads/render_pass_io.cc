// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/common/quads/render_pass_io.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bit_cast.h"
#include "base/containers/span.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/values.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace gfx {
struct HDRMetadata;
}

namespace viz {

namespace {

enum RenderPassField {
  kRenderPassID = 1 << 0,
  kRenderPassOutputRect = 1 << 1,
  kRenderPassDamageRect = 1 << 2,
  kRenderPassTransformToRootTarget = 1 << 3,
  kRenderPassFilters = 1 << 4,
  kRenderPassBackdropFilters = 1 << 5,
  kRenderPassBackdropFilterBounds = 1 << 6,
  kRenderPassColorSpace = 1 << 7,
  kRenderPassHasTransparentBackground = 1 << 8,
  kRenderPassCacheRenderPass = 1 << 9,
  kRenderPassHasDamageFromContributingContent = 1 << 10,
  kRenderPassGenerateMipmap = 1 << 11,
  kRenderPassCopyRequests = 1 << 12,
  kRenderPassQuadList = 1 << 13,
  kRenderPassSharedQuadStateList = 1 << 14,
  kRenderPassHasPreQuadDamage = 1 << 15,
  kRenderPassAllFields = 0xFFFFFFFF,
};

// This controls which fields are processed in CompositorRenderPassToDict() and
// CompositorRenderPassFromDict().
// Values other than kAllFields should never be checked in. This is only for
// local debugging convenience.
RenderPassField g_render_pass_fields = kRenderPassAllFields;

bool ProcessRenderPassField(RenderPassField field) {
  return (g_render_pass_fields & field) == field;
}

base::Value::Dict RectToDict(const gfx::Rect& rect) {
  return base::Value::Dict()
      .Set("x", rect.x())
      .Set("y", rect.y())
      .Set("width", rect.width())
      .Set("height", rect.height());
}

bool RectFromDict(const base::Value::Dict& dict, gfx::Rect* rect) {
  DCHECK(rect);
  std::optional<int> x = dict.FindInt("x");
  std::optional<int> y = dict.FindInt("y");
  std::optional<int> width = dict.FindInt("width");
  std::optional<int> height = dict.FindInt("height");
  if (!x || !y || !width || !height) {
    return false;
  }
  rect->SetRect(x.value(), y.value(), width.value(), height.value());
  return true;
}

base::Value::Dict RectFToDict(const gfx::RectF& rect) {
  return base::Value::Dict()
      .Set("x", rect.x())
      .Set("y", rect.y())
      .Set("width", rect.width())
      .Set("height", rect.height());
}

bool RectFFromDict(const base::Value::Dict& dict, gfx::RectF* rect) {
  DCHECK(rect);
  std::optional<double> x = dict.FindDouble("x");
  std::optional<double> y = dict.FindDouble("y");
  std::optional<double> width = dict.FindDouble("width");
  std::optional<double> height = dict.FindDouble("height");
  if (!x || !y || !width || !height) {
    return false;
  }
  rect->SetRect(static_cast<float>(x.value()), static_cast<float>(y.value()),
                static_cast<float>(width.value()),
                static_cast<float>(height.value()));
  return true;
}

base::Value::Dict SizeToDict(const gfx::Size& size) {
  return base::Value::Dict()
      .Set("width", size.width())
      .Set("height", size.height());
}

bool SizeFromDict(const base::Value::Dict& dict, gfx::Size* size) {
  DCHECK(size);
  std::optional<int> width = dict.FindInt("width");
  std::optional<int> height = dict.FindInt("height");
  if (!width || !height) {
    return false;
  }
  size->set_width(width.value());
  size->set_height(height.value());
  return true;
}

base::Value::Dict PointToDict(const gfx::Point& point) {
  return base::Value::Dict().Set("x", point.x()).Set("y", point.y());
}

bool PointFromDict(const base::Value::Dict& dict, gfx::Point* point) {
  DCHECK(point);
  std::optional<int> x = dict.FindInt("x");
  std::optional<int> y = dict.FindInt("y");
  if (!x || !y) {
    return false;
  }
  point->set_x(x.value());
  point->set_y(y.value());
  return true;
}

base::Value::Dict SkColor4fToDict(const SkColor4f color) {
  return base::Value::Dict()
      .Set("red", color.fR)
      .Set("green", color.fG)
      .Set("blue", color.fB)
      .Set("alpha", color.fA);
}

bool SkColor4fFromDict(const base::Value::Dict& dict, SkColor4f* color) {
  DCHECK(color);
  std::optional<double> red = dict.FindDouble("red");
  std::optional<double> green = dict.FindDouble("green");
  std::optional<double> blue = dict.FindDouble("blue");
  std::optional<double> alpha = dict.FindDouble("alpha");
  if (!red || !green || !blue || !alpha)
    return false;
  color->fR = static_cast<float>(red.value());
  color->fG = static_cast<float>(green.value());
  color->fB = static_cast<float>(blue.value());
  color->fA = static_cast<float>(alpha.value());
  return true;
}

// Many quads now store color as an SkColor4f, but older logs will still store
// SkColors (which are ints). For backward compatibility's sake, read either.
bool ColorFromDict(const base::Value::Dict& dict,
                   std::string_view key,
                   SkColor4f* output_color) {
  const base::Value::Dict* color_key = dict.FindDict(key);
  SkColor4f color_4f;
  if (!color_key || !SkColor4fFromDict(*color_key, &color_4f)) {
    std::optional<int> color_int = dict.FindInt(key);
    if (!color_int)
      return false;
    color_4f = SkColor4f::FromColor(static_cast<SkColor>(color_int.value()));
  }
  output_color->fR = color_4f.fR;
  output_color->fG = color_4f.fG;
  output_color->fB = color_4f.fB;
  output_color->fA = color_4f.fA;
  return true;
}

base::Value::Dict PointFToDict(const gfx::PointF& point) {
  return base::Value::Dict().Set("x", point.x()).Set("y", point.y());
}

bool PointFFromDict(const base::Value::Dict& dict, gfx::PointF* point) {
  DCHECK(point);
  std::optional<double> x = dict.FindDouble("x");
  std::optional<double> y = dict.FindDouble("y");
  if (!x || !y) {
    return false;
  }
  point->set_x(static_cast<float>(x.value()));
  point->set_y(static_cast<float>(y.value()));
  return true;
}

base::Value::Dict Vector2dFToDict(const gfx::Vector2dF& v) {
  return PointFToDict(gfx::PointF(v.x(), v.y()));
}

bool Vector2dFFromDict(const base::Value::Dict& dict, gfx::Vector2dF* v) {
  DCHECK(v);
  gfx::PointF point;
  if (!PointFFromDict(dict, &point))
    return false;

  v->set_x(point.x());
  v->set_y(point.y());
  return true;
}

base::Value::List FloatArrayToList(base::span<const float> data) {
  base::Value::List list;
  for (float num : data)
    list.Append(num);
  return list;
}

bool FloatArrayFromList(const base::Value::List& list,
                        size_t expected_count,
                        float* data) {
  DCHECK(data);
  DCHECK_LT(0u, expected_count);
  size_t count = list.size();
  if (count != expected_count)
    return false;
  std::vector<double> double_data(count);
  for (size_t ii = 0; ii < count; ++ii) {
    if (!list[ii].is_double())
      return false;
    double_data[ii] = list[ii].GetDouble();
  }
  for (size_t ii = 0; ii < count; ++ii)
    data[ii] = static_cast<float>(double_data[ii]);
  return true;
}

#define MAP_RRECTF_TYPE_TO_STRING(NAME) \
  case gfx::RRectF::Type::NAME:         \
    return #NAME;
const char* RRectFTypeToString(gfx::RRectF::Type type) {
  switch (type) {
    MAP_RRECTF_TYPE_TO_STRING(kEmpty)
    MAP_RRECTF_TYPE_TO_STRING(kRect)
    MAP_RRECTF_TYPE_TO_STRING(kSingle)
    MAP_RRECTF_TYPE_TO_STRING(kSimple)
    MAP_RRECTF_TYPE_TO_STRING(kOval)
    MAP_RRECTF_TYPE_TO_STRING(kComplex)
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}
#undef MAP_RRECTF_TYPE_TO_STRING

#define MAP_STRING_TO_RRECTF_TYPE(NAME) \
  if (str == #NAME)                     \
    return static_cast<int>(gfx::RRectF::Type::NAME);
int StringToRRectFType(const std::string& str) {
  MAP_STRING_TO_RRECTF_TYPE(kEmpty)
  MAP_STRING_TO_RRECTF_TYPE(kRect)
  MAP_STRING_TO_RRECTF_TYPE(kSingle)
  MAP_STRING_TO_RRECTF_TYPE(kSimple)
  MAP_STRING_TO_RRECTF_TYPE(kOval)
  MAP_STRING_TO_RRECTF_TYPE(kComplex)
  return -1;
}
#undef MAP_STRING_TO_RRECTF_TYPE

base::Value::Dict RRectFToDict(const gfx::RRectF& rect) {
  base::Value::Dict dict;
  dict.Set("type", RRectFTypeToString(rect.GetType()));
  if (rect.GetType() != gfx::RRectF::Type::kEmpty) {
    dict.Set("rect", RectFToDict(rect.rect()));
    dict.Set("upper_left.x",
             rect.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).x());
    dict.Set("upper_left.y",
             rect.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).y());
    dict.Set("upper_right.x",
             rect.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).x());
    dict.Set("upper_right.y",
             rect.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).y());
    dict.Set("lower_right.x",
             rect.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).x());
    dict.Set("lower_right.y",
             rect.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).y());
    dict.Set("lower_left.x",
             rect.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).x());
    dict.Set("lower_left.y",
             rect.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).y());
  }
  return dict;
}

bool RRectFFromDict(const base::Value::Dict& dict, gfx::RRectF* out) {
  DCHECK(out);
  const std::string* type = dict.FindString("type");
  if (!type)
    return false;
  int type_index = StringToRRectFType(*type);
  if (type_index < 0)
    return false;
  gfx::RRectF::Type t_type = static_cast<gfx::RRectF::Type>(type_index);
  if (t_type == gfx::RRectF::Type::kEmpty) {
    *out = gfx::RRectF();
    DCHECK_EQ(gfx::RRectF::Type::kEmpty, out->GetType());
    return true;
  }
  const base::Value::Dict* rect = dict.FindDict("rect");
  std::optional<double> upper_left_x = dict.FindDouble("upper_left.x");
  std::optional<double> upper_left_y = dict.FindDouble("upper_left.y");
  std::optional<double> upper_right_x = dict.FindDouble("upper_right.x");
  std::optional<double> upper_right_y = dict.FindDouble("upper_right.y");
  std::optional<double> lower_right_x = dict.FindDouble("lower_right.x");
  std::optional<double> lower_right_y = dict.FindDouble("lower_right.y");
  std::optional<double> lower_left_x = dict.FindDouble("lower_left.x");
  std::optional<double> lower_left_y = dict.FindDouble("lower_left.y");
  if (!rect || !upper_left_x || !upper_left_y || !upper_right_x ||
      !upper_right_y || !lower_right_x || !lower_right_y || !lower_left_x ||
      !lower_left_y) {
    return false;
  }
  gfx::RectF rect_f;
  if (!RectFFromDict(*rect, &rect_f))
    return false;
  gfx::RRectF rrectf(rect_f, static_cast<float>(upper_left_x.value()),
                     static_cast<float>(upper_left_y.value()),
                     static_cast<float>(upper_right_x.value()),
                     static_cast<float>(upper_right_y.value()),
                     static_cast<float>(lower_right_x.value()),
                     static_cast<float>(lower_right_y.value()),
                     static_cast<float>(lower_left_x.value()),
                     static_cast<float>(lower_left_y.value()));
  if (rrectf.GetType() != t_type)
    return false;
  *out = rrectf;
  return true;
}

base::Value::Dict LinearGradientToDict(
    const gfx::LinearGradient& gradient_mask) {
  base::Value::List steps;
  for (size_t i = 0; i < gradient_mask.step_count(); ++i) {
    steps.Append(
        base::Value::Dict()
            .Set("fraction",
                 static_cast<double>(gradient_mask.steps()[i].fraction))
            .Set("alpha", static_cast<int>(gradient_mask.steps()[i].alpha)));
  }

  return base::Value::Dict()
      .Set("angle", static_cast<double>(gradient_mask.angle()))
      .Set("step_count", static_cast<int>(gradient_mask.step_count()))
      .Set("steps", std::move(steps));
}

bool LinearGradientFromDict(const base::Value::Dict& dict,
                            gfx::LinearGradient* out) {
  std::optional<double> angle = dict.FindDouble("angle");
  std::optional<int> step_count = dict.FindInt("step_count");
  if (!angle || !step_count)
    return false;

  gfx::LinearGradient gradient_mask = gfx::LinearGradient(*angle);
  const base::Value::List* steps = dict.FindList("steps");
  if (!steps)
    return false;
  for (const base::Value& v : *steps) {
    const base::Value::Dict* step = v.GetIfDict();
    if (!step)
      return false;

    std::optional<double> fraction = step->FindDouble("fraction");
    std::optional<int> alpha = step->FindInt("alpha");
    if (!fraction || !alpha)
      return false;

    gradient_mask.AddStep(*fraction, *alpha);
  }

  *out = gradient_mask;
  return true;
}

base::Value::Dict MaskFilterInfoToDict(
    const gfx::MaskFilterInfo& mask_filter_info) {
  auto dict = base::Value::Dict().Set(
      "rounded_corner_bounds",
      RRectFToDict(mask_filter_info.rounded_corner_bounds()));
  if (mask_filter_info.HasGradientMask()) {
    dict.Set("gradient_mask",
             LinearGradientToDict(*mask_filter_info.gradient_mask()));
  }
  return dict;
}

bool MaskFilterInfoFromDict(const base::Value::Dict& dict,
                            gfx::MaskFilterInfo* out) {
  DCHECK(out);
  const base::Value::Dict* rounded_corner_bounds =
      dict.FindDict("rounded_corner_bounds");
  if (!rounded_corner_bounds)
    return false;
  gfx::RRectF t_rounded_corner_bounds;
  if (!RRectFFromDict(*rounded_corner_bounds, &t_rounded_corner_bounds))
    return false;

  const base::Value::Dict* gradient_mask = dict.FindDict("gradient_mask");
  if (!gradient_mask) {
    *out = gfx::MaskFilterInfo(t_rounded_corner_bounds);
    return true;
  }

  gfx::LinearGradient t_gradient_mask;
  if (!LinearGradientFromDict(*gradient_mask, &t_gradient_mask))
    return false;

  *out = gfx::MaskFilterInfo(t_rounded_corner_bounds, t_gradient_mask);
  return true;
}

base::Value::List TransformToList(const gfx::Transform& transform) {
  base::Value::List list;
  float data[16];
  transform.GetColMajorF(data);
  for (float value : data)
    list.Append(value);
  return list;
}

bool TransformFromList(const base::Value::List& list,
                       gfx::Transform* transform) {
  DCHECK(transform);
  if (list.size() != 16)
    return false;
  float data[16];
  for (size_t ii = 0; ii < 16; ++ii) {
    if (!list[ii].is_double())
      return false;
    data[ii] = list[ii].GetDouble();
  }
  *transform = gfx::Transform::ColMajorF(data);
  return true;
}

base::Value::List ShapeRectsToList(
    const cc::FilterOperation::ShapeRects& shape) {
  base::Value::List list;
  for (const auto& ii : shape) {
    list.Append(RectToDict(ii));
  }
  return list;
}

bool ShapeRectsFromList(const base::Value::List& list,
                        cc::FilterOperation::ShapeRects* shape) {
  DCHECK(shape);
  size_t size = list.size();
  cc::FilterOperation::ShapeRects data;
  data.resize(size);
  for (size_t ii = 0; ii < size; ++ii) {
    const base::Value& dict_value = list[ii];
    if (!dict_value.is_dict())
      return false;
    if (!RectFromDict(dict_value.GetDict(), &data[ii]))
      return false;
  }
  *shape = data;
  return true;
}

std::string PaintFilterToString(const sk_sp<cc::PaintFilter>& filter) {
  // TODO(zmo): Expand to readable fields. Such recorded data becomes invalid
  // when we update any data structure.
  std::vector<uint8_t> buffer(cc::PaintOpWriter::SerializedSize(filter.get()));
  // No need to populate the SerializeOptions here since the security
  // constraints explicitly disable serializing images using the transfer cache
  // and serialization of PaintRecords.
  cc::PaintOp::SerializeOptions options;
  cc::PaintOpWriter writer(buffer.data(), buffer.size(), options,
                           true /* enable_security_constraints */);
  writer.Write(filter.get(), SkM44());
  if (writer.size() == 0)
    return "";
  buffer.resize(writer.size());

  return base::Base64Encode(buffer);
}

sk_sp<cc::PaintFilter> PaintFilterFromString(const std::string& encoded) {
  if (encoded.empty()) {
    return nullptr;
  }

  std::string buffer;
  if (!base::Base64Decode(encoded, &buffer))
    return nullptr;

  // We don't need to populate the DeserializeOptions here since the security
  // constraints explicitly disable serializing images using the transfer cache
  // and serialization of PaintRecords.
  std::vector<uint8_t> scratch_buffer;
  cc::PaintOp::DeserializeOptions options{.scratch_buffer = scratch_buffer};
  cc::PaintOpReader reader(buffer.data(), buffer.size(), options,
                           /*enable_security_constraints=*/true);
  sk_sp<cc::PaintFilter> filter;
  reader.Read(&filter);
  if (!reader.valid())
    return nullptr;
  // We must have consumed all bytes written when reading this filter.
  if (reader.remaining_bytes() != 0u)
    return nullptr;

  return filter;
}

base::Value::Dict FilterOperationToDict(const cc::FilterOperation& filter) {
  base::Value::Dict dict;
  cc::FilterOperation::FilterType type = filter.type();

  dict.Set("type", type);
  if (type != cc::FilterOperation::COLOR_MATRIX &&
      type != cc::FilterOperation::REFERENCE &&
      type != cc::FilterOperation::OFFSET) {
    dict.Set("amount", filter.amount());
  }
  switch (type) {
    case cc::FilterOperation::ALPHA_THRESHOLD:
      dict.Set("shape", ShapeRectsToList(filter.shape()));
      break;
    case cc::FilterOperation::DROP_SHADOW:
      dict.Set("offset", PointToDict(filter.offset()));
      dict.Set("drop_shadow_color",
               SkColor4fToDict(filter.drop_shadow_color()));
      break;
    case cc::FilterOperation::REFERENCE:
      dict.Set("image_filter", PaintFilterToString(filter.image_filter()));
      break;
    case cc::FilterOperation::COLOR_MATRIX:
      dict.Set("matrix", FloatArrayToList(filter.matrix()));
      break;
    case cc::FilterOperation::ZOOM:
      dict.Set("zoom_inset", filter.zoom_inset());
      break;
    case cc::FilterOperation::BLUR:
      dict.Set("blur_tile_mode", static_cast<int>(filter.blur_tile_mode()));
      break;
    case cc::FilterOperation::OFFSET:
      dict.Set("offset", PointToDict(filter.offset()));
      break;
    default:
      break;
  }
  return dict;
}

bool FilterOperationFromDict(const base::Value& dict_value,
                             cc::FilterOperation* out) {
  DCHECK(out);
  if (!dict_value.is_dict()) {
    return false;
  }

  const base::Value::Dict& dict = dict_value.GetDict();
  std::optional<int> type = dict.FindInt("type");
  std::optional<double> amount = dict.FindDouble("amount");
  const base::Value::Dict* offset = dict.FindDict("offset");
  const std::string* image_filter = dict.FindString("image_filter");
  const base::Value::List* matrix = dict.FindList("matrix");
  std::optional<int> zoom_inset = dict.FindInt("zoom_inset");
  const base::Value::List* shape = dict.FindList("shape");
  std::optional<int> blur_tile_mode = dict.FindInt("blur_tile_mode");

  cc::FilterOperation filter;

  if (!type)
    return false;
  cc::FilterOperation::FilterType filter_type =
      static_cast<cc::FilterOperation::FilterType>(type.value());
  filter.set_type(filter_type);
  if (filter_type != cc::FilterOperation::COLOR_MATRIX &&
      filter_type != cc::FilterOperation::REFERENCE &&
      filter_type != cc::FilterOperation::OFFSET) {
    if (!amount)
      return false;
    filter.set_amount(static_cast<float>(amount.value()));
  }
  switch (filter_type) {
    case cc::FilterOperation::ALPHA_THRESHOLD: {
      cc::FilterOperation::ShapeRects shape_rects;
      if (!shape || !ShapeRectsFromList(*shape, &shape_rects)) {
        return false;
      }
      filter.set_shape(shape_rects);
    } break;
    case cc::FilterOperation::DROP_SHADOW: {
      gfx::Point drop_shadow_offset;
      if (!offset || !PointFromDict(*offset, &drop_shadow_offset)) {
        return false;
      }
      filter.set_offset(drop_shadow_offset);

      SkColor4f t_drop_shadow_color;
      if (!ColorFromDict(dict, "drop_shadow_color", &t_drop_shadow_color)) {
        return false;
      }
      filter.set_drop_shadow_color(t_drop_shadow_color);
    } break;
    case cc::FilterOperation::REFERENCE:
      if (!image_filter)
        return false;
      filter.set_image_filter(PaintFilterFromString(*image_filter));
      break;
    case cc::FilterOperation::COLOR_MATRIX: {
      cc::FilterOperation::Matrix mat;
      if (!matrix || !FloatArrayFromList(*matrix, 20u, &mat[0]))
        return false;
      filter.set_matrix(mat);
    } break;
    case cc::FilterOperation::ZOOM:
      if (!zoom_inset)
        return false;
      filter.set_zoom_inset(zoom_inset.value());
      break;
    case cc::FilterOperation::BLUR:
      if (!blur_tile_mode)
        return false;
      filter.set_blur_tile_mode(
          static_cast<SkTileMode>(blur_tile_mode.value()));
      break;
    case cc::FilterOperation::OFFSET: {
      gfx::Point filter_offset;
      if (!offset || !PointFromDict(*offset, &filter_offset)) {
        return false;
      }
      filter.set_offset(filter_offset);
    } break;
    default:
      break;
  }

  *out = filter;
  return true;
}

base::Value::List FilterOperationsToList(const cc::FilterOperations& filters) {
  base::Value::List list;
  for (size_t ii = 0; ii < filters.size(); ++ii) {
    base::Value::Dict filter_dict = FilterOperationToDict(filters.at(ii));
    list.Append(std::move(filter_dict));
  }
  return list;
}

bool FilterOperationsFromList(const base::Value::List& list,
                              cc::FilterOperations* filters) {
  DCHECK(filters);
  cc::FilterOperations data;
  for (const auto& entry : list) {
    cc::FilterOperation filter;
    if (!FilterOperationFromDict(entry, &filter))
      return false;
    data.Append(filter);
  }
  *filters = data;
  return true;
}

#define MATCH_ENUM_CASE(TYPE, NAME) \
  case gfx::ColorSpace::TYPE::NAME: \
    return #NAME;

const char* ColorSpacePrimaryIdToString(gfx::ColorSpace::PrimaryID id) {
  switch (id) {
    MATCH_ENUM_CASE(PrimaryID, INVALID)
    MATCH_ENUM_CASE(PrimaryID, BT709)
    MATCH_ENUM_CASE(PrimaryID, BT470M)
    MATCH_ENUM_CASE(PrimaryID, BT470BG)
    MATCH_ENUM_CASE(PrimaryID, SMPTE170M)
    MATCH_ENUM_CASE(PrimaryID, SMPTE240M)
    MATCH_ENUM_CASE(PrimaryID, FILM)
    MATCH_ENUM_CASE(PrimaryID, BT2020)
    MATCH_ENUM_CASE(PrimaryID, SMPTEST428_1)
    MATCH_ENUM_CASE(PrimaryID, SMPTEST431_2)
    MATCH_ENUM_CASE(PrimaryID, P3)
    MATCH_ENUM_CASE(PrimaryID, XYZ_D50)
    MATCH_ENUM_CASE(PrimaryID, ADOBE_RGB)
    MATCH_ENUM_CASE(PrimaryID, APPLE_GENERIC_RGB)
    MATCH_ENUM_CASE(PrimaryID, WIDE_GAMUT_COLOR_SPIN)
    MATCH_ENUM_CASE(PrimaryID, CUSTOM)
    MATCH_ENUM_CASE(PrimaryID, EBU_3213_E)
  }
}

const char* ColorSpaceTransferIdToString(gfx::ColorSpace::TransferID id) {
  switch (id) {
    MATCH_ENUM_CASE(TransferID, INVALID)
    MATCH_ENUM_CASE(TransferID, BT709)
    MATCH_ENUM_CASE(TransferID, BT709_APPLE)
    MATCH_ENUM_CASE(TransferID, GAMMA18)
    MATCH_ENUM_CASE(TransferID, GAMMA22)
    MATCH_ENUM_CASE(TransferID, GAMMA24)
    MATCH_ENUM_CASE(TransferID, GAMMA28)
    MATCH_ENUM_CASE(TransferID, SMPTE170M)
    MATCH_ENUM_CASE(TransferID, SMPTE240M)
    MATCH_ENUM_CASE(TransferID, LINEAR)
    MATCH_ENUM_CASE(TransferID, LOG)
    MATCH_ENUM_CASE(TransferID, LOG_SQRT)
    MATCH_ENUM_CASE(TransferID, IEC61966_2_4)
    MATCH_ENUM_CASE(TransferID, BT1361_ECG)
    MATCH_ENUM_CASE(TransferID, SRGB)
    MATCH_ENUM_CASE(TransferID, BT2020_10)
    MATCH_ENUM_CASE(TransferID, BT2020_12)
    MATCH_ENUM_CASE(TransferID, PQ)
    MATCH_ENUM_CASE(TransferID, SMPTEST428_1)
    MATCH_ENUM_CASE(TransferID, HLG)
    MATCH_ENUM_CASE(TransferID, SRGB_HDR)
    MATCH_ENUM_CASE(TransferID, LINEAR_HDR)
    MATCH_ENUM_CASE(TransferID, CUSTOM)
    MATCH_ENUM_CASE(TransferID, CUSTOM_HDR)
    MATCH_ENUM_CASE(TransferID, PIECEWISE_HDR)
    MATCH_ENUM_CASE(TransferID, SCRGB_LINEAR_80_NITS)
  }
}

const char* ColorSpaceMatrixIdToString(gfx::ColorSpace::MatrixID id) {
  switch (id) {
    MATCH_ENUM_CASE(MatrixID, INVALID)
    MATCH_ENUM_CASE(MatrixID, RGB)
    MATCH_ENUM_CASE(MatrixID, BT709)
    MATCH_ENUM_CASE(MatrixID, FCC)
    MATCH_ENUM_CASE(MatrixID, BT470BG)
    MATCH_ENUM_CASE(MatrixID, SMPTE170M)
    MATCH_ENUM_CASE(MatrixID, SMPTE240M)
    MATCH_ENUM_CASE(MatrixID, YCOCG)
    MATCH_ENUM_CASE(MatrixID, BT2020_NCL)
    MATCH_ENUM_CASE(MatrixID, YDZDX)
    MATCH_ENUM_CASE(MatrixID, GBR)
  }
}

const char* ColorSpaceRangeIdToString(gfx::ColorSpace::RangeID id) {
  switch (id) {
    MATCH_ENUM_CASE(RangeID, INVALID)
    MATCH_ENUM_CASE(RangeID, LIMITED)
    MATCH_ENUM_CASE(RangeID, FULL)
    MATCH_ENUM_CASE(RangeID, DERIVED)
  }
}
#undef MATCH_ENUM_CASE

#define MATCH_ENUM_CASE(TYPE, NAME) \
  if (token == #NAME)               \
    return static_cast<uint8_t>(gfx::ColorSpace::TYPE::NAME);

uint8_t StringToColorSpacePrimaryId(const std::string& token) {
  MATCH_ENUM_CASE(PrimaryID, INVALID)
  MATCH_ENUM_CASE(PrimaryID, BT709)
  MATCH_ENUM_CASE(PrimaryID, BT470M)
  MATCH_ENUM_CASE(PrimaryID, BT470BG)
  MATCH_ENUM_CASE(PrimaryID, SMPTE170M)
  MATCH_ENUM_CASE(PrimaryID, SMPTE240M)
  MATCH_ENUM_CASE(PrimaryID, FILM)
  MATCH_ENUM_CASE(PrimaryID, BT2020)
  MATCH_ENUM_CASE(PrimaryID, SMPTEST428_1)
  MATCH_ENUM_CASE(PrimaryID, SMPTEST431_2)
  MATCH_ENUM_CASE(PrimaryID, P3)
  MATCH_ENUM_CASE(PrimaryID, XYZ_D50)
  MATCH_ENUM_CASE(PrimaryID, ADOBE_RGB)
  MATCH_ENUM_CASE(PrimaryID, APPLE_GENERIC_RGB)
  MATCH_ENUM_CASE(PrimaryID, WIDE_GAMUT_COLOR_SPIN)
  MATCH_ENUM_CASE(PrimaryID, CUSTOM)
  MATCH_ENUM_CASE(PrimaryID, EBU_3213_E)
  return -1;
}

uint8_t StringToColorSpaceTransferId(const std::string& token) {
  MATCH_ENUM_CASE(TransferID, INVALID)
  MATCH_ENUM_CASE(TransferID, BT709)
  MATCH_ENUM_CASE(TransferID, BT709_APPLE)
  MATCH_ENUM_CASE(TransferID, GAMMA18)
  MATCH_ENUM_CASE(TransferID, GAMMA22)
  MATCH_ENUM_CASE(TransferID, GAMMA24)
  MATCH_ENUM_CASE(TransferID, GAMMA28)
  MATCH_ENUM_CASE(TransferID, SMPTE170M)
  MATCH_ENUM_CASE(TransferID, SMPTE240M)
  MATCH_ENUM_CASE(TransferID, LINEAR)
  MATCH_ENUM_CASE(TransferID, LOG)
  MATCH_ENUM_CASE(TransferID, LOG_SQRT)
  MATCH_ENUM_CASE(TransferID, IEC61966_2_4)
  MATCH_ENUM_CASE(TransferID, BT1361_ECG)
  MATCH_ENUM_CASE(TransferID, SRGB)
  MATCH_ENUM_CASE(TransferID, BT2020_10)
  MATCH_ENUM_CASE(TransferID, BT2020_12)
  MATCH_ENUM_CASE(TransferID, PQ)
  MATCH_ENUM_CASE(TransferID, SMPTEST428_1)
  MATCH_ENUM_CASE(TransferID, HLG)
  MATCH_ENUM_CASE(TransferID, SRGB_HDR)
  MATCH_ENUM_CASE(TransferID, LINEAR_HDR)
  MATCH_ENUM_CASE(TransferID, CUSTOM)
  MATCH_ENUM_CASE(TransferID, CUSTOM_HDR)
  MATCH_ENUM_CASE(TransferID, PIECEWISE_HDR)
  MATCH_ENUM_CASE(TransferID, SCRGB_LINEAR_80_NITS)
  return -1;
}

uint8_t StringToColorSpaceMatrixId(const std::string& token) {
  MATCH_ENUM_CASE(MatrixID, INVALID)
  MATCH_ENUM_CASE(MatrixID, RGB)
  MATCH_ENUM_CASE(MatrixID, BT709)
  MATCH_ENUM_CASE(MatrixID, FCC)
  MATCH_ENUM_CASE(MatrixID, BT470BG)
  MATCH_ENUM_CASE(MatrixID, SMPTE170M)
  MATCH_ENUM_CASE(MatrixID, SMPTE240M)
  MATCH_ENUM_CASE(MatrixID, YCOCG)
  MATCH_ENUM_CASE(MatrixID, BT2020_NCL)
  MATCH_ENUM_CASE(MatrixID, YDZDX)
  MATCH_ENUM_CASE(MatrixID, GBR)
  return -1;
}

uint8_t StringToColorSpaceRangeId(const std::string& token) {
  MATCH_ENUM_CASE(RangeID, INVALID)
  MATCH_ENUM_CASE(RangeID, LIMITED)
  MATCH_ENUM_CASE(RangeID, FULL)
  MATCH_ENUM_CASE(RangeID, DERIVED)
  return -1;
}
#undef MATCH_ENUM_CASE

base::Value::List Matrix3x3ToList(const skcms_Matrix3x3& mat) {
  float data[9];
  memcpy(data, mat.vals, sizeof(mat));
  return FloatArrayToList(data);
}

bool Matrix3x3FromList(const base::Value::List& list, skcms_Matrix3x3* mat) {
  DCHECK(mat);
  return FloatArrayFromList(list, 9u, reinterpret_cast<float*>(mat->vals));
}

base::Value::List TransferFunctionToList(const skcms_TransferFunction& fn) {
  float data[7];
  data[0] = fn.a;
  data[1] = fn.b;
  data[2] = fn.c;
  data[3] = fn.d;
  data[4] = fn.e;
  data[5] = fn.f;
  data[6] = fn.g;
  return FloatArrayToList(data);
}

bool TransferFunctionFromList(const base::Value::List& list,
                              skcms_TransferFunction* fn) {
  DCHECK(fn);
  float data[7];
  if (!FloatArrayFromList(list, 7u, data))
    return false;
  fn->a = data[0];
  fn->b = data[1];
  fn->c = data[2];
  fn->d = data[3];
  fn->e = data[4];
  fn->f = data[5];
  fn->g = data[6];
  return true;
}

base::Value::Dict ColorSpaceToDict(const gfx::ColorSpace& color_space) {
  auto dict =
      base::Value::Dict()
          .Set("primaries",
               ColorSpacePrimaryIdToString(color_space.GetPrimaryID()))
          .Set("transfer",
               ColorSpaceTransferIdToString(color_space.GetTransferID()))
          .Set("matrix", ColorSpaceMatrixIdToString(color_space.GetMatrixID()))
          .Set("range", ColorSpaceRangeIdToString(color_space.GetRangeID()));
  if (color_space.GetPrimaryID() == gfx::ColorSpace::PrimaryID::CUSTOM) {
    skcms_Matrix3x3 mat;
    color_space.GetPrimaryMatrix(&mat);
    dict.Set("custom_primary_matrix", Matrix3x3ToList(mat));
  }
  if (color_space.GetTransferID() == gfx::ColorSpace::TransferID::CUSTOM ||
      color_space.GetTransferID() == gfx::ColorSpace::TransferID::CUSTOM_HDR) {
    skcms_TransferFunction fn;
    color_space.GetTransferFunction(&fn);
    dict.Set("custom_transfer_params", TransferFunctionToList(fn));
  }
  return dict;
}

bool ColorSpaceFromDict(const base::Value::Dict& dict,
                        gfx::ColorSpace* color_space) {
  DCHECK(color_space);
  const std::string* primaries = dict.FindString("primaries");
  const std::string* transfer = dict.FindString("transfer");
  const std::string* matrix = dict.FindString("matrix");
  const std::string* range = dict.FindString("range");
  if (!primaries || !transfer || !matrix || !range)
    return false;
  uint8_t primary_id = StringToColorSpacePrimaryId(*primaries);
  uint8_t transfer_id = StringToColorSpaceTransferId(*transfer);
  uint8_t matrix_id = StringToColorSpaceMatrixId(*matrix);
  uint8_t range_id = StringToColorSpaceRangeId(*range);
  if (primary_id < 0 || transfer_id < 0 || matrix_id < 0 || range_id < 0)
    return false;
  skcms_Matrix3x3 t_custom_primary_matrix;
  bool uses_custom_primary_matrix =
      primary_id == static_cast<uint8_t>(gfx::ColorSpace::PrimaryID::CUSTOM);
  if (uses_custom_primary_matrix) {
    const base::Value::List* custom_primary_matrix =
        dict.FindList("custom_primary_matrix");
    if (!custom_primary_matrix ||
        !Matrix3x3FromList(*custom_primary_matrix, &t_custom_primary_matrix)) {
      return false;
    }
  }
  skcms_TransferFunction t_custom_transfer_params;
  bool uses_custom_transfer_params =
      transfer_id ==
          static_cast<uint8_t>(gfx::ColorSpace::TransferID::CUSTOM) ||
      transfer_id ==
          static_cast<uint8_t>(gfx::ColorSpace::TransferID::CUSTOM_HDR);
  if (uses_custom_transfer_params) {
    const base::Value::List* custom_transfer_params =
        dict.FindList("custom_transfer_params");
    if (!custom_transfer_params ||
        !TransferFunctionFromList(*custom_transfer_params,
                                  &t_custom_transfer_params)) {
      return false;
    }
  }
  *color_space = gfx::ColorSpace(
      static_cast<gfx::ColorSpace::PrimaryID>(primary_id),
      static_cast<gfx::ColorSpace::TransferID>(transfer_id),
      static_cast<gfx::ColorSpace::MatrixID>(matrix_id),
      static_cast<gfx::ColorSpace::RangeID>(range_id),
      uses_custom_primary_matrix ? &t_custom_primary_matrix : nullptr,
      uses_custom_transfer_params ? &t_custom_transfer_params : nullptr);
  return true;
}

base::Value::List DrawQuadResourcesToList(
    const DrawQuad::Resources& resources) {
  base::Value::List list;
  DCHECK_LE(resources.count, DrawQuad::Resources::kMaxResourceIdCount);
  for (ResourceId id : resources)
    list.Append(static_cast<int>(id.GetUnsafeValue()));
  return list;
}

bool DrawQuadResourcesFromList(const base::Value::List& list,
                               DrawQuad::Resources* resources) {
  DCHECK(resources);
  size_t size = list.size();
  if (size == 0u) {
    resources->count = 0u;
    return true;
  }
  if (size > DrawQuad::Resources::kMaxResourceIdCount)
    return false;
  for (size_t ii = 0; ii < size; ++ii) {
    if (!list[ii].is_int())
      return false;
  }

  resources->count = static_cast<uint32_t>(size);
  for (size_t ii = 0; ii < size; ++ii) {
    resources->ids[ii] = ResourceId(list[ii].GetInt());
  }
  return true;
}

base::Value::Dict SurfaceIdToDict(const SurfaceId& id) {
  return base::Value::Dict()
      .Set("client_id", static_cast<int>(id.frame_sink_id().client_id()))
      .Set("sink_id", static_cast<int>(id.frame_sink_id().sink_id()))
      .Set("parent_seq",
           static_cast<int>(id.local_surface_id().parent_sequence_number()))
      .Set("child_seq",
           static_cast<int>(id.local_surface_id().child_sequence_number()))
      .Set("embed_token",
           base::UnguessableTokenToValue(id.local_surface_id().embed_token()));
}

std::optional<SurfaceId> SurfaceIdFromDict(const base::Value::Dict& dict) {
  std::optional<int> client_id = dict.FindInt("client_id");
  std::optional<int> sink_id = dict.FindInt("sink_id");
  std::optional<int> parent_seq = dict.FindInt("parent_seq");
  std::optional<int> child_seq = dict.FindInt("child_seq");
  const base::Value* embed_token_value = dict.Find("embed_token");
  if (!client_id || !sink_id || !parent_seq || !child_seq || !embed_token_value)
    return std::nullopt;

  auto token = base::ValueToUnguessableToken(*embed_token_value);
  if (!token) {
    return std::nullopt;
  }

  return SurfaceId(FrameSinkId(*client_id, *sink_id),
                   LocalSurfaceId(*parent_seq, *child_seq, *token));
}

base::Value::Dict SurfaceRangeToDict(const SurfaceRange& range) {
  base::Value::Dict dict;
  if (range.start().has_value())
    dict.Set("start", SurfaceIdToDict(*(range.start())));
  dict.Set("end", SurfaceIdToDict(range.end()));
  return dict;
}

std::optional<SurfaceRange> SurfaceRangeFromDict(
    const base::Value::Dict& dict) {
  const base::Value::Dict* start_dict = dict.FindDict("start");
  const base::Value::Dict* end_dict = dict.FindDict("end");
  if (!end_dict)
    return std::nullopt;
  std::optional<SurfaceId> start =
      start_dict ? SurfaceIdFromDict(*start_dict) : std::nullopt;
  std::optional<SurfaceId> end = SurfaceIdFromDict(*end_dict);
  if (!end || (start_dict && !start))
    return std::nullopt;

  return SurfaceRange(start, *end);
}

int GetSharedQuadStateIndex(const SharedQuadStateList& shared_quad_state_list,
                            const SharedQuadState* shared_quad_state) {
  for (auto iter = shared_quad_state_list.begin();
       iter != shared_quad_state_list.end(); ++iter) {
    if (*iter == shared_quad_state)
      return static_cast<int>(iter.index());
  }
  return -1;
}

#define MAP_STRING_TO_MATERIAL(NAME) \
  if (str == #NAME)                  \
    return static_cast<int>(DrawQuad::Material::NAME);
int StringToDrawQuadMaterial(const std::string& str) {
  MAP_STRING_TO_MATERIAL(kInvalid)
  MAP_STRING_TO_MATERIAL(kDebugBorder)
  MAP_STRING_TO_MATERIAL(kPictureContent)
  MAP_STRING_TO_MATERIAL(kCompositorRenderPass)
  MAP_STRING_TO_MATERIAL(kSharedElement)
  MAP_STRING_TO_MATERIAL(kSolidColor)
  MAP_STRING_TO_MATERIAL(kSurfaceContent)
  MAP_STRING_TO_MATERIAL(kTextureContent)
  MAP_STRING_TO_MATERIAL(kTiledContent)
  MAP_STRING_TO_MATERIAL(kVideoHole)
  return -1;
}
#undef MAP_STRING_TO_MATERIAL

void DrawQuadCommonToDict(const DrawQuad* draw_quad,
                          base::Value::Dict* dict,
                          const SharedQuadStateList& shared_quad_state_list) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->Set("material", DrawQuadMaterialToString(draw_quad->material));
  dict->Set("rect", RectToDict(draw_quad->rect));
  dict->Set("visible_rect", RectToDict(draw_quad->visible_rect));
  dict->Set("needs_blending", draw_quad->needs_blending);
  int shared_quad_state_index = GetSharedQuadStateIndex(
      shared_quad_state_list, draw_quad->shared_quad_state);
  DCHECK_LE(0, shared_quad_state_index);
  dict->Set("shared_quad_state_index", shared_quad_state_index);
  dict->Set("resources", DrawQuadResourcesToList(draw_quad->resources));
}

void ContentDrawQuadCommonToDict(const ContentDrawQuadBase* draw_quad,
                                 base::Value::Dict* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->Set("tex_coord_rect", RectFToDict(draw_quad->tex_coord_rect));
  dict->Set("texture_size", SizeToDict(draw_quad->texture_size));
  dict->Set("is_premultiplied", draw_quad->is_premultiplied);
  dict->Set("nearest_neighbor", draw_quad->nearest_neighbor);
  dict->Set("force_anti_aliasing_off", draw_quad->force_anti_aliasing_off);
}

struct DrawQuadCommon {
  DrawQuad::Material material = DrawQuad::Material::kInvalid;
  gfx::Rect rect;
  gfx::Rect visible_rect;
  bool needs_blending = false;
  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of speedometer3).
  RAW_PTR_EXCLUSION const SharedQuadState* shared_quad_state = nullptr;
  DrawQuad::Resources resources;
};

std::optional<DrawQuadCommon> GetDrawQuadCommonFromDict(
    const base::Value::Dict& dict,
    const SharedQuadStateList& shared_quad_state_list) {
  const std::string* material = dict.FindString("material");
  const base::Value::Dict* rect = dict.FindDict("rect");
  const base::Value::Dict* visible_rect = dict.FindDict("visible_rect");
  std::optional<bool> needs_blending = dict.FindBool("needs_blending");
  std::optional<int> shared_quad_state_index =
      dict.FindInt("shared_quad_state_index");
  const base::Value::List* resources = dict.FindList("resources");
  if (!material || !rect || !visible_rect || !needs_blending ||
      !shared_quad_state_index || !resources) {
    return std::nullopt;
  }
  int material_index = StringToDrawQuadMaterial(*material);
  if (material_index < 0)
    return std::nullopt;
  int sqs_index = shared_quad_state_index.value();
  if (sqs_index < 0 ||
      static_cast<size_t>(sqs_index) >= shared_quad_state_list.size()) {
    return std::nullopt;
  }
  gfx::Rect t_rect, t_visible_rect;
  if (!RectFromDict(*rect, &t_rect) ||
      !RectFromDict(*visible_rect, &t_visible_rect)) {
    return std::nullopt;
  }
  DrawQuad::Resources t_resources;
  if (!DrawQuadResourcesFromList(*resources, &t_resources))
    return std::nullopt;

  return DrawQuadCommon{static_cast<DrawQuad::Material>(material_index),
                        t_rect,
                        t_visible_rect,
                        needs_blending.value(),
                        shared_quad_state_list.ElementAt(sqs_index),
                        t_resources};
}

struct ContentDrawQuadCommon {
  gfx::RectF tex_coord_rect;
  gfx::Size texture_size;
  bool is_premultiplied;
  bool nearest_neighbor;
  bool force_anti_aliasing_off;
};

std::optional<ContentDrawQuadCommon> GetContentDrawQuadCommonFromDict(
    const base::Value::Dict& dict) {
  const base::Value::Dict* tex_coord_rect = dict.FindDict("tex_coord_rect");
  const base::Value::Dict* texture_size = dict.FindDict("texture_size");
  std::optional<bool> is_premultiplied = dict.FindBool("is_premultiplied");
  std::optional<bool> nearest_neighbor = dict.FindBool("nearest_neighbor");
  std::optional<bool> force_anti_aliasing_off =
      dict.FindBool("force_anti_aliasing_off");

  if (!tex_coord_rect || !texture_size || !is_premultiplied ||
      !nearest_neighbor || !force_anti_aliasing_off) {
    return std::nullopt;
  }
  gfx::RectF t_tex_coord_rect;
  gfx::Size t_texture_size;
  if (!RectFFromDict(*tex_coord_rect, &t_tex_coord_rect) ||
      !SizeFromDict(*texture_size, &t_texture_size)) {
    return std::nullopt;
  }

  return ContentDrawQuadCommon{
      t_tex_coord_rect, t_texture_size, is_premultiplied.value(),
      nearest_neighbor.value(), force_anti_aliasing_off.value()};
}

void CompositorRenderPassDrawQuadToDict(
    const CompositorRenderPassDrawQuad* draw_quad,
    base::Value::Dict* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->Set(
      "render_pass_id",
      base::NumberToString(static_cast<uint64_t>(draw_quad->render_pass_id)));
  dict->Set("mask_uv_rect", RectFToDict(draw_quad->mask_uv_rect));
  dict->Set("mask_texture_size", SizeToDict(draw_quad->mask_texture_size));
  dict->Set("filters_scale", Vector2dFToDict(draw_quad->filters_scale));
  dict->Set("filters_origin", PointFToDict(draw_quad->filters_origin));
  dict->Set("tex_coord_rect", RectFToDict(draw_quad->tex_coord_rect));
  dict->Set("backdrop_filter_quality", draw_quad->backdrop_filter_quality);
  dict->Set("force_anti_aliasing_off", draw_quad->force_anti_aliasing_off);
  dict->Set("intersects_damage_under", draw_quad->intersects_damage_under);
  DCHECK_GE(1u, draw_quad->resources.count);
}

void SolidColorDrawQuadToDict(const SolidColorDrawQuad* draw_quad,
                              base::Value::Dict* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->Set("color", SkColor4fToDict(draw_quad->color));
  dict->Set("force_anti_aliasing_off", draw_quad->force_anti_aliasing_off);
}

#define MAP_VIDEO_TYPE_TO_STRING(NAME) \
  case gfx::ProtectedVideoType::NAME:  \
    return #NAME;
const char* ProtectedVideoTypeToString(gfx::ProtectedVideoType type) {
  switch (type) {
    MAP_VIDEO_TYPE_TO_STRING(kClear)
    MAP_VIDEO_TYPE_TO_STRING(kSoftwareProtected)
    MAP_VIDEO_TYPE_TO_STRING(kHardwareProtected)
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}
#undef MAP_VIDEO_TYPE_TO_STRING

#define MAP_STRING_TO_VIDEO_TYPE(NAME) \
  if (str == #NAME)                    \
    return static_cast<int>(gfx::ProtectedVideoType::NAME);
int StringToProtectedVideoType(const std::string& str) {
  MAP_STRING_TO_VIDEO_TYPE(kClear)
  MAP_STRING_TO_VIDEO_TYPE(kSoftwareProtected)
  MAP_STRING_TO_VIDEO_TYPE(kHardwareProtected)
  return -1;
}
#undef MAP_STRING_TO_VIDEO_TYPE

void SurfaceDrawQuadToDict(const SurfaceDrawQuad* draw_quad,
                           base::Value::Dict* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->Set("surface_range", SurfaceRangeToDict(draw_quad->surface_range));
  dict->Set("default_background_color",
            SkColor4fToDict(draw_quad->default_background_color));
  dict->Set("stretch_content", draw_quad->stretch_content_to_fill_bounds);
  dict->Set("is_reflection", draw_quad->is_reflection);
  dict->Set("allow_merge", draw_quad->allow_merge);
}

void TextureDrawQuadToDict(const TextureDrawQuad* draw_quad,
                           base::Value::Dict* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->Set("premultiplied_alpha", draw_quad->premultiplied_alpha);
  dict->Set("uv_top_left", PointFToDict(draw_quad->uv_top_left));
  dict->Set("uv_bottom_right", PointFToDict(draw_quad->uv_bottom_right));
  dict->Set("background_color", SkColor4fToDict(draw_quad->background_color));
  // TODO(crbug.com/40942150): Update
  // "components/test/data/viz/render_pass_data/" to reflect the deprecation of
  // vertex opacity.
  float vertex_opacity[4] = {1.f, 1.0f, 1.0f, 1.f};
  dict->Set("vertex_opacity", FloatArrayToList(vertex_opacity));
  dict->Set("y_flipped", draw_quad->y_flipped);
  dict->Set("nearest_neighbor", draw_quad->nearest_neighbor);
  dict->Set("secure_output_only", draw_quad->secure_output_only);
  dict->Set("protected_video_type",
            ProtectedVideoTypeToString(draw_quad->protected_video_type));
  DCHECK_EQ(1u, draw_quad->resources.count);
  dict->Set("resource_size_in_pixels",
            SizeToDict(draw_quad->overlay_resources.size_in_pixels));
  if (draw_quad->damage_rect.has_value()) {
    dict->Set("damage_rect", RectToDict(draw_quad->damage_rect.value()));
  }
  // Conditionally set is_stream_video to not break backwards-compatibility with
  // unit test data.
  // Note: is_video_frame is not being saved in dict.
  if (draw_quad->is_stream_video) {
    dict->Set("is_stream_video", draw_quad->is_stream_video);
  }
}

void TileDrawQuadToDict(const TileDrawQuad* draw_quad,
                        base::Value::Dict* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  ContentDrawQuadCommonToDict(draw_quad, dict);
  DCHECK_EQ(1u, draw_quad->resources.count);
}

void VideoHoleDrawQuadToDict(const VideoHoleDrawQuad* draw_quad,
                             base::Value::Dict* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->Set("overlay_plane_id.empty", draw_quad->overlay_plane_id.is_empty());
  if (!draw_quad->overlay_plane_id.is_empty()) {
    dict->Set("overlay_plane_id.unguessable_token",
              base::UnguessableTokenToValue(draw_quad->overlay_plane_id));
  }
}

#define UNEXPECTED_DRAW_QUAD_TYPE(NAME)                  \
  case DrawQuad::Material::NAME:                         \
    NOTREACHED_IN_MIGRATION() << "Unexpected " << #NAME; \
    break;
#define WRITE_DRAW_QUAD_TYPE_FIELDS(NAME, TYPE)                    \
  case DrawQuad::Material::NAME:                                   \
    TYPE##ToDict(reinterpret_cast<const TYPE*>(draw_quad), &dict); \
    break;
base::Value::Dict DrawQuadToDict(
    const DrawQuad* draw_quad,
    const SharedQuadStateList& shared_quad_state_list) {
  DCHECK(draw_quad);
  base::Value::Dict dict;
  DrawQuadCommonToDict(draw_quad, &dict, shared_quad_state_list);
  switch (draw_quad->material) {
    WRITE_DRAW_QUAD_TYPE_FIELDS(kCompositorRenderPass,
                                CompositorRenderPassDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kSolidColor, SolidColorDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kSurfaceContent, SurfaceDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kTextureContent, TextureDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kTiledContent, TileDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kVideoHole, VideoHoleDrawQuad)
    UNEXPECTED_DRAW_QUAD_TYPE(kPictureContent)
    default:
      break;
  }
  return dict;
}
#undef WRITE_DRAW_QUAD_TYPE_FIELDS
#undef UNEXPECTED_DRAW_QUAD_TYPE

base::Value::List QuadListToList(
    const QuadList& quad_list,
    const SharedQuadStateList& shared_quad_state_list) {
  base::Value::List list;
  for (size_t ii = 0; ii < quad_list.size(); ++ii) {
    list.Append(
        DrawQuadToDict(quad_list.ElementAt(ii), shared_quad_state_list));
  }
  return list;
}

bool CompositorRenderPassDrawQuadFromDict(
    const base::Value::Dict& dict,
    const DrawQuadCommon& common,
    CompositorRenderPassDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  if (common.resources.count > 1u)
    return false;

  const std::string* render_pass_id = dict.FindString("render_pass_id");
  const base::Value::Dict* mask_uv_rect = dict.FindDict("mask_uv_rect");
  const base::Value::Dict* mask_texture_size =
      dict.FindDict("mask_texture_size");
  const base::Value::Dict* filters_scale = dict.FindDict("filters_scale");
  const base::Value::Dict* filters_origin = dict.FindDict("filters_origin");
  const base::Value::Dict* tex_coord_rect = dict.FindDict("tex_coord_rect");
  std::optional<double> backdrop_filter_quality =
      dict.FindDouble("backdrop_filter_quality");
  std::optional<bool> force_anti_aliasing_off =
      dict.FindBool("force_anti_aliasing_off");
  std::optional<bool> intersects_damage_under =
      dict.FindBool("intersects_damage_under");

  if (!render_pass_id || !mask_uv_rect || !mask_texture_size ||
      !filters_scale || !filters_origin || !tex_coord_rect ||
      !backdrop_filter_quality || !force_anti_aliasing_off) {
    return false;
  }
  uint64_t render_pass_id_as_int;
  gfx::RectF t_mask_uv_rect, t_tex_coord_rect;
  gfx::Size t_mask_texture_size;
  gfx::Vector2dF t_filters_scale;
  gfx::PointF t_filters_origin;
  if (!base::StringToUint64(*render_pass_id, &render_pass_id_as_int) ||
      !RectFFromDict(*mask_uv_rect, &t_mask_uv_rect) ||
      !SizeFromDict(*mask_texture_size, &t_mask_texture_size) ||
      !Vector2dFFromDict(*filters_scale, &t_filters_scale) ||
      !PointFFromDict(*filters_origin, &t_filters_origin) ||
      !RectFFromDict(*tex_coord_rect, &t_tex_coord_rect)) {
    return false;
  }
  CompositorRenderPassId t_render_pass_id{render_pass_id_as_int};

  ResourceId mask_resource_id = kInvalidResourceId;
  if (common.resources.count == 1u) {
    const size_t kIndex = CompositorRenderPassDrawQuad::kMaskResourceIdIndex;
    mask_resource_id = common.resources.ids[kIndex];
  }
  draw_quad->SetAll(
      common.shared_quad_state, common.rect, common.visible_rect,
      common.needs_blending, t_render_pass_id, mask_resource_id, t_mask_uv_rect,
      t_mask_texture_size, t_filters_scale, t_filters_origin, t_tex_coord_rect,
      force_anti_aliasing_off.value(), backdrop_filter_quality.value(),
      intersects_damage_under ? intersects_damage_under.value() : false);
  return true;
}

bool SolidColorDrawQuadFromDict(const base::Value::Dict& dict,
                                const DrawQuadCommon& common,
                                SolidColorDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  std::optional<bool> force_anti_aliasing_off =
      dict.FindBool("force_anti_aliasing_off");
  if (!force_anti_aliasing_off)
    return false;

  SkColor4f t_color;
  if (!ColorFromDict(dict, "color", &t_color))
    return false;

  draw_quad->SetAll(common.shared_quad_state, common.rect, common.visible_rect,
                    common.needs_blending, t_color,
                    force_anti_aliasing_off.value());
  return true;
}

bool SurfaceDrawQuadFromDict(const base::Value::Dict& dict,
                             const DrawQuadCommon& common,
                             SurfaceDrawQuad* draw_quad) {
  DCHECK(draw_quad);

  const base::Value::Dict* surface_range_dict = dict.FindDict("surface_range");
  if (!surface_range_dict)
    return false;
  std::optional<SurfaceRange> surface_range =
      SurfaceRangeFromDict(*surface_range_dict);
  std::optional<bool> stretch_content = dict.FindBool("stretch_content");
  std::optional<bool> is_reflection = dict.FindBool("is_reflection");
  std::optional<bool> allow_merge = dict.FindBool("allow_merge");
  if (!surface_range || !stretch_content || !is_reflection || !allow_merge)
    return false;

  SkColor4f t_default_background_color;
  if (!ColorFromDict(dict, "default_background_color",
                     &t_default_background_color)) {
    return false;
  }

  draw_quad->SetAll(common.shared_quad_state, common.rect, common.visible_rect,
                    common.needs_blending, *surface_range,
                    t_default_background_color, *stretch_content,
                    *is_reflection, *allow_merge);
  return true;
}

bool TextureDrawQuadFromDict(const base::Value::Dict& dict,
                             const DrawQuadCommon& common,
                             TextureDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  if (common.resources.count != 1u)
    return false;

  std::optional<bool> premultiplied_alpha =
      dict.FindBool("premultiplied_alpha");
  const base::Value::Dict* uv_top_left = dict.FindDict("uv_top_left");
  const base::Value::Dict* uv_bottom_right = dict.FindDict("uv_bottom_right");
  // TODO(crbug.com/40942150): Update
  // "components/test/data/viz/render_pass_data/" to reflect the deprecation of
  // vertex opacity.
  const base::Value::List* vertex_opacity = dict.FindList("vertex_opacity");
  const base::Value::Dict* damage_rect = dict.FindDict("damage_rect");
  std::optional<bool> y_flipped = dict.FindBool("y_flipped");
  std::optional<bool> nearest_neighbor = dict.FindBool("nearest_neighbor");
  std::optional<bool> secure_output_only = dict.FindBool("secure_output_only");
  const std::string* protected_video_type =
      dict.FindString("protected_video_type");
  const base::Value::Dict* resource_size_in_pixels =
      dict.FindDict("resource_size_in_pixels");

  if (!premultiplied_alpha || !uv_top_left || !uv_bottom_right ||
      !vertex_opacity || !y_flipped || !nearest_neighbor ||
      !secure_output_only || !protected_video_type ||
      !resource_size_in_pixels) {
    return false;
  }
  int protected_video_type_index =
      StringToProtectedVideoType(*protected_video_type);
  if (protected_video_type_index < 0)
    return false;
  gfx::PointF t_uv_top_left, t_uv_bottom_right;
  gfx::Size t_resource_size_in_pixels;
  SkColor4f t_background_color;
  if (!PointFFromDict(*uv_top_left, &t_uv_top_left) ||
      !PointFFromDict(*uv_bottom_right, &t_uv_bottom_right) ||
      !SizeFromDict(*resource_size_in_pixels, &t_resource_size_in_pixels) ||
      !ColorFromDict(dict, "background_color", &t_background_color)) {
    return false;
  }

  const size_t kIndex = TextureDrawQuad::kResourceIdIndex;
  ResourceId resource_id = common.resources.ids[kIndex];
  draw_quad->SetAll(
      common.shared_quad_state, common.rect, common.visible_rect,
      common.needs_blending, resource_id, t_resource_size_in_pixels,
      premultiplied_alpha.value(), t_uv_top_left, t_uv_bottom_right,
      t_background_color, y_flipped.value(), nearest_neighbor.value(),
      secure_output_only.value(),
      static_cast<gfx::ProtectedVideoType>(protected_video_type_index));

  draw_quad->is_stream_video = dict.FindBool("is_stream_video").value_or(false);

  gfx::Rect t_damage_rect;
  if (damage_rect && RectFromDict(*damage_rect, &t_damage_rect)) {
    draw_quad->damage_rect = t_damage_rect;
  }

  return true;
}

bool TileDrawQuadFromDict(const base::Value::Dict& dict,
                          const DrawQuadCommon& common,
                          TileDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  if (common.resources.count != 1u)
    return false;

  std::optional<ContentDrawQuadCommon> content_common =
      GetContentDrawQuadCommonFromDict(dict);
  if (!content_common)
    return false;

  const size_t kIndex = TileDrawQuad::kResourceIdIndex;
  ResourceId resource_id = common.resources.ids[kIndex];

  draw_quad->SetAll(
      common.shared_quad_state, common.rect, common.visible_rect,
      common.needs_blending, resource_id, content_common->tex_coord_rect,
      content_common->texture_size, content_common->is_premultiplied,
      content_common->nearest_neighbor,
      content_common->force_anti_aliasing_off);
  return true;
}

bool VideoHoleDrawQuadFromDict(const base::Value::Dict& dict,
                               const DrawQuadCommon& common,
                               VideoHoleDrawQuad* draw_quad) {
  DCHECK(draw_quad);

  std::optional<bool> overlay_plane_id_empty =
      dict.FindBool("overlay_plane_id.empty");
  if (!overlay_plane_id_empty)
    return false;

  base::UnguessableToken overlay_plane_id;
  DCHECK(overlay_plane_id.is_empty());
  if (!overlay_plane_id_empty.value()) {
    std::optional<base::UnguessableToken> deserialized_overlay_plane_id =
        base::ValueToUnguessableToken(
            dict.Find("overlay_plane_id.unguessable_token"));
    if (!deserialized_overlay_plane_id) {
      return false;
    }
    overlay_plane_id = deserialized_overlay_plane_id.value();
  }
  draw_quad->SetAll(common.shared_quad_state, common.rect, common.visible_rect,
                    common.needs_blending, overlay_plane_id);
  return true;
}

#define UNEXPECTED_DRAW_QUAD_TYPE(NAME)                  \
  case DrawQuad::Material::NAME:                         \
    NOTREACHED_IN_MIGRATION() << "Unexpected " << #NAME; \
    break;
#define GET_QUAD_FROM_DICT(NAME, TYPE)                             \
  case DrawQuad::Material::NAME: {                                 \
    TYPE* quad = quads.AllocateAndConstruct<TYPE>();               \
    if (!list[ii].is_dict())                                       \
      return false;                                                \
    if (!TYPE##FromDict(list[ii].GetDict(), common.value(), quad)) \
      return false;                                                \
  } break;
bool QuadListFromList(const base::Value::List& list,
                      QuadList* quad_list,
                      const SharedQuadStateList& shared_quad_state_list) {
  DCHECK(quad_list);
  size_t size = list.size();
  if (size == 0) {
    quad_list->clear();
    return true;
  }
  QuadList quads(size);
  for (size_t ii = 0; ii < size; ++ii) {
    if (!list[ii].is_dict())
      return false;
    std::optional<DrawQuadCommon> common =
        GetDrawQuadCommonFromDict(list[ii].GetDict(), shared_quad_state_list);
    if (!common)
      return false;
    switch (common->material) {
      GET_QUAD_FROM_DICT(kCompositorRenderPass, CompositorRenderPassDrawQuad)
      GET_QUAD_FROM_DICT(kSolidColor, SolidColorDrawQuad)
      GET_QUAD_FROM_DICT(kSurfaceContent, SurfaceDrawQuad)
      GET_QUAD_FROM_DICT(kTextureContent, TextureDrawQuad)
      GET_QUAD_FROM_DICT(kTiledContent, TileDrawQuad)
      GET_QUAD_FROM_DICT(kVideoHole, VideoHoleDrawQuad)
      UNEXPECTED_DRAW_QUAD_TYPE(kPictureContent)
      default:
        break;
    }
  }
  quad_list->swap(quads);
  return true;
}
#undef GET_QUAD_FROM_DICT
#undef UNEXPECTED_DRAW_QUAD_TYPE

base::Value::Dict SharedQuadStateToDict(const SharedQuadState& sqs) {
  auto dict =
      base::Value::Dict()
          .Set("quad_to_target_transform",
               TransformToList(sqs.quad_to_target_transform))
          .Set("quad_layer_rect", RectToDict(sqs.quad_layer_rect))
          .Set("visible_quad_layer_rect",
               RectToDict(sqs.visible_quad_layer_rect))
          .Set("mask_filter_info", MaskFilterInfoToDict(sqs.mask_filter_info))
          .Set("are_contents_opaque", sqs.are_contents_opaque)
          .Set("opacity", sqs.opacity)
          .Set("blend_mode", BlendModeToString(sqs.blend_mode))
          .Set("sorting_context_id", sqs.sorting_context_id)
          .Set("is_fast_rounded_corner", sqs.is_fast_rounded_corner);
  if (sqs.clip_rect) {
    dict.Set("clip_rect", RectToDict(*sqs.clip_rect));
  }
  return dict;
}

#define MAP_STRING_TO_BLEND_MODE(NAME) \
  if (str == #NAME)                    \
    return static_cast<int>(SkBlendMode::NAME);
int StringToBlendMode(const std::string& str) {
  MAP_STRING_TO_BLEND_MODE(kClear)
  MAP_STRING_TO_BLEND_MODE(kSrc)
  MAP_STRING_TO_BLEND_MODE(kDst)
  MAP_STRING_TO_BLEND_MODE(kSrcOver)
  MAP_STRING_TO_BLEND_MODE(kDstOver)
  MAP_STRING_TO_BLEND_MODE(kSrcIn)
  MAP_STRING_TO_BLEND_MODE(kDstIn)
  MAP_STRING_TO_BLEND_MODE(kSrcOut)
  MAP_STRING_TO_BLEND_MODE(kDstOut)
  MAP_STRING_TO_BLEND_MODE(kSrcATop)
  MAP_STRING_TO_BLEND_MODE(kDstATop)
  MAP_STRING_TO_BLEND_MODE(kXor)
  MAP_STRING_TO_BLEND_MODE(kPlus)
  MAP_STRING_TO_BLEND_MODE(kModulate)
  MAP_STRING_TO_BLEND_MODE(kScreen)
  MAP_STRING_TO_BLEND_MODE(kOverlay)
  MAP_STRING_TO_BLEND_MODE(kDarken)
  MAP_STRING_TO_BLEND_MODE(kLighten)
  MAP_STRING_TO_BLEND_MODE(kColorDodge)
  MAP_STRING_TO_BLEND_MODE(kColorBurn)
  MAP_STRING_TO_BLEND_MODE(kHardLight)
  MAP_STRING_TO_BLEND_MODE(kSoftLight)
  MAP_STRING_TO_BLEND_MODE(kDifference)
  MAP_STRING_TO_BLEND_MODE(kExclusion)
  MAP_STRING_TO_BLEND_MODE(kMultiply)
  MAP_STRING_TO_BLEND_MODE(kHue)
  MAP_STRING_TO_BLEND_MODE(kSaturation)
  MAP_STRING_TO_BLEND_MODE(kColor)
  MAP_STRING_TO_BLEND_MODE(kLuminosity)
  return -1;
}
#undef MAP_STRING_TO_BLEND_MODE

bool SharedQuadStateFromDict(const base::Value::Dict& dict,
                             SharedQuadState* sqs) {
  DCHECK(sqs);
  const base::Value::List* quad_to_target_transform =
      dict.FindList("quad_to_target_transform");
  const base::Value::Dict* quad_layer_rect = dict.FindDict("quad_layer_rect");
  const base::Value::Dict* visible_quad_layer_rect =
      dict.FindDict("visible_quad_layer_rect");
  const base::Value::Dict* mask_filter_info = dict.FindDict("mask_filter_info");
  const base::Value::Dict* clip_rect = dict.FindDict("clip_rect");
  std::optional<bool> is_clipped = dict.FindBool("is_clipped");
  std::optional<bool> are_contents_opaque =
      dict.FindBool("are_contents_opaque");
  std::optional<double> opacity = dict.FindDouble("opacity");
  const std::string* blend_mode = dict.FindString("blend_mode");
  std::optional<int> sorting_context_id = dict.FindInt("sorting_context_id");
  std::optional<bool> is_fast_rounded_corner =
      dict.FindBool("is_fast_rounded_corner");

  if (!quad_to_target_transform || !quad_layer_rect ||
      !visible_quad_layer_rect || !are_contents_opaque || !opacity ||
      !blend_mode || !sorting_context_id || !is_fast_rounded_corner) {
    return false;
  }
  gfx::Transform t_quad_to_target_transform;
  gfx::Rect t_quad_layer_rect, t_visible_quad_layer_rect, t_clip_rect;
  if (!TransformFromList(*quad_to_target_transform,
                         &t_quad_to_target_transform) ||
      !RectFromDict(*quad_layer_rect, &t_quad_layer_rect) ||
      !RectFromDict(*visible_quad_layer_rect, &t_visible_quad_layer_rect) ||
      (clip_rect && !RectFromDict(*clip_rect, &t_clip_rect))) {
    return false;
  }

  gfx::MaskFilterInfo t_mask_filter_info;
  if (mask_filter_info &&
      !MaskFilterInfoFromDict(*mask_filter_info, &t_mask_filter_info)) {
    return false;
  }

  std::optional<gfx::Rect> clip_rect_opt;
  // Some older files still use the is_clipped field.  If it's present, we'll
  // respect it, and ignore clip_rect if it's false.
  if (is_clipped.has_value()) {
    if (is_clipped.value()) {
      clip_rect_opt = t_clip_rect;
    }
  } else if (clip_rect) {
    clip_rect_opt = t_clip_rect;
  }

  int blend_mode_index = StringToBlendMode(*blend_mode);
  DCHECK_GE(static_cast<int>(SkBlendMode::kLastMode), blend_mode_index);
  if (blend_mode_index < 0)
    return false;
  SkBlendMode t_blend_mode = static_cast<SkBlendMode>(blend_mode_index);
  sqs->SetAll(t_quad_to_target_transform, t_quad_layer_rect,
              t_visible_quad_layer_rect, t_mask_filter_info, clip_rect_opt,
              are_contents_opaque.value(), static_cast<float>(opacity.value()),
              t_blend_mode, sorting_context_id.value(), /*layer_id=*/0u,
              is_fast_rounded_corner.value());
  return true;
}

base::Value::List SharedQuadStateListToList(
    const SharedQuadStateList& shared_quad_state_list) {
  base::Value::List list;
  for (size_t ii = 0; ii < shared_quad_state_list.size(); ++ii)
    list.Append(SharedQuadStateToDict(*(shared_quad_state_list.ElementAt(ii))));
  return list;
}

bool SharedQuadStateListFromList(const base::Value::List& list,
                                 SharedQuadStateList* shared_quad_state_list) {
  DCHECK(shared_quad_state_list);
  size_t size = list.size();
  SharedQuadStateList states(alignof(SharedQuadState), sizeof(SharedQuadState),
                             size);
  for (size_t ii = 0; ii < size; ++ii) {
    if (!list[ii].is_dict())
      return false;
    SharedQuadState* sqs = states.AllocateAndConstruct<SharedQuadState>();
    if (!SharedQuadStateFromDict(list[ii].GetDict(), sqs))
      return false;
  }
  shared_quad_state_list->swap(states);
  return true;
}

base::Value::Dict GetRenderPassMetadata(
    const CompositorRenderPass& render_pass) {
  return base::Value::Dict()
      .Set("render_pass_id",
           base::NumberToString(static_cast<uint64_t>(render_pass.id)))
      .Set("quad_count", static_cast<int>(render_pass.quad_list.size()))
      .Set("shared_quad_state_count",
           static_cast<int>(render_pass.shared_quad_state_list.size()));
}

base::Value::List GetRenderPassListMetadata(
    const CompositorRenderPassList& render_pass_list) {
  base::Value::List metadata;
  for (const auto& render_pass : render_pass_list) {
    metadata.Append(GetRenderPassMetadata(*(render_pass.get())));
  }
  return metadata;
}

}  // namespace

#define MAP_BLEND_MODE_TO_STRING(NAME) \
  case SkBlendMode::NAME:              \
    return #NAME;
const char* BlendModeToString(SkBlendMode blend_mode) {
  switch (blend_mode) {
    MAP_BLEND_MODE_TO_STRING(kClear)
    MAP_BLEND_MODE_TO_STRING(kSrc)
    MAP_BLEND_MODE_TO_STRING(kDst)
    MAP_BLEND_MODE_TO_STRING(kSrcOver)
    MAP_BLEND_MODE_TO_STRING(kDstOver)
    MAP_BLEND_MODE_TO_STRING(kSrcIn)
    MAP_BLEND_MODE_TO_STRING(kDstIn)
    MAP_BLEND_MODE_TO_STRING(kSrcOut)
    MAP_BLEND_MODE_TO_STRING(kDstOut)
    MAP_BLEND_MODE_TO_STRING(kSrcATop)
    MAP_BLEND_MODE_TO_STRING(kDstATop)
    MAP_BLEND_MODE_TO_STRING(kXor)
    MAP_BLEND_MODE_TO_STRING(kPlus)
    MAP_BLEND_MODE_TO_STRING(kModulate)
    MAP_BLEND_MODE_TO_STRING(kScreen)
    MAP_BLEND_MODE_TO_STRING(kOverlay)
    MAP_BLEND_MODE_TO_STRING(kDarken)
    MAP_BLEND_MODE_TO_STRING(kLighten)
    MAP_BLEND_MODE_TO_STRING(kColorDodge)
    MAP_BLEND_MODE_TO_STRING(kColorBurn)
    MAP_BLEND_MODE_TO_STRING(kHardLight)
    MAP_BLEND_MODE_TO_STRING(kSoftLight)
    MAP_BLEND_MODE_TO_STRING(kDifference)
    MAP_BLEND_MODE_TO_STRING(kExclusion)
    MAP_BLEND_MODE_TO_STRING(kMultiply)
    MAP_BLEND_MODE_TO_STRING(kHue)
    MAP_BLEND_MODE_TO_STRING(kSaturation)
    MAP_BLEND_MODE_TO_STRING(kColor)
    MAP_BLEND_MODE_TO_STRING(kLuminosity)
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}
#undef MAP_BLEND_MODE_TO_STRING

#define MAP_MATERIAL_TO_STRING(NAME) \
  case DrawQuad::Material::NAME:     \
    return #NAME;
const char* DrawQuadMaterialToString(DrawQuad::Material material) {
  switch (material) {
    MAP_MATERIAL_TO_STRING(kInvalid)
    MAP_MATERIAL_TO_STRING(kDebugBorder)
    MAP_MATERIAL_TO_STRING(kPictureContent)
    MAP_MATERIAL_TO_STRING(kCompositorRenderPass)
    MAP_MATERIAL_TO_STRING(kSharedElement)
    MAP_MATERIAL_TO_STRING(kSolidColor)
    MAP_MATERIAL_TO_STRING(kSurfaceContent)
    MAP_MATERIAL_TO_STRING(kTextureContent)
    MAP_MATERIAL_TO_STRING(kTiledContent)
    MAP_MATERIAL_TO_STRING(kVideoHole)
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}
#undef MAP_MATERIAL_TO_STRING

base::Value::Dict CompositorRenderPassToDict(
    const CompositorRenderPass& render_pass) {
  base::Value::Dict dict;
  if (ProcessRenderPassField(kRenderPassID))
    dict.Set("id", base::NumberToString(static_cast<uint64_t>(render_pass.id)));
  if (ProcessRenderPassField(kRenderPassOutputRect))
    dict.Set("output_rect", RectToDict(render_pass.output_rect));
  if (ProcessRenderPassField(kRenderPassDamageRect))
    dict.Set("damage_rect", RectToDict(render_pass.damage_rect));
  if (ProcessRenderPassField(kRenderPassTransformToRootTarget)) {
    dict.Set("transform_to_root_target",
             TransformToList(render_pass.transform_to_root_target));
  }
  if (ProcessRenderPassField(kRenderPassFilters))
    dict.Set("filters", FilterOperationsToList(render_pass.filters));
  if (ProcessRenderPassField(kRenderPassBackdropFilters)) {
    dict.Set("backdrop_filters",
             FilterOperationsToList(render_pass.backdrop_filters));
  }
  if (ProcessRenderPassField(kRenderPassBackdropFilterBounds) &&
      render_pass.backdrop_filter_bounds) {
    dict.Set("backdrop_filter_bounds",
             RRectFToDict(render_pass.backdrop_filter_bounds.value()));
  }
  if (ProcessRenderPassField(kRenderPassColorSpace)) {
    // CompositorRenderPasses used to have a color space field, but this was
    // removed in favor of color usage. https://crbug.com/1049334
    gfx::ColorSpace render_pass_color_space = gfx::ColorSpace::CreateSRGB();
    dict.Set("color_space", ColorSpaceToDict(render_pass_color_space));
  }
  if (ProcessRenderPassField(kRenderPassHasTransparentBackground)) {
    dict.Set("has_transparent_background",
             render_pass.has_transparent_background);
  }
  if (ProcessRenderPassField(kRenderPassCacheRenderPass))
    dict.Set("cache_render_pass", render_pass.cache_render_pass);
  if (ProcessRenderPassField(kRenderPassHasPreQuadDamage)) {
    // Set the dict value only if it is not the non default value.
    if (render_pass.has_per_quad_damage)
      dict.Set("has_per_quad_damage", render_pass.has_per_quad_damage);
  }
  if (ProcessRenderPassField(kRenderPassHasDamageFromContributingContent)) {
    dict.Set("has_damage_from_contributing_content",
             render_pass.has_damage_from_contributing_content);
  }
  if (ProcessRenderPassField(kRenderPassGenerateMipmap))
    dict.Set("generate_mipmap", render_pass.generate_mipmap);
  if (ProcessRenderPassField(kRenderPassCopyRequests)) {
    // TODO(zmo): Write copy_requests.
  }
  if (ProcessRenderPassField(kRenderPassQuadList)) {
    dict.Set("quad_list", QuadListToList(render_pass.quad_list,
                                         render_pass.shared_quad_state_list));
  }
  if (ProcessRenderPassField(kRenderPassSharedQuadStateList)) {
    dict.Set("shared_quad_state_list",
             SharedQuadStateListToList(render_pass.shared_quad_state_list));
  }
  return dict;
}

std::unique_ptr<CompositorRenderPass> CompositorRenderPassFromDict(
    const base::Value::Dict& dict) {
  auto pass = CompositorRenderPass::Create();

  if (ProcessRenderPassField(kRenderPassID)) {
    const std::string* id = dict.FindString("id");
    if (!id)
      return nullptr;
    uint64_t pass_id_as_int = 0;
    if (!base::StringToUint64(*id, &pass_id_as_int))
      return nullptr;
    pass->id = CompositorRenderPassId{pass_id_as_int};
  }

  if (ProcessRenderPassField(kRenderPassOutputRect)) {
    const base::Value::Dict* output_rect = dict.FindDict("output_rect");
    if (!output_rect)
      return nullptr;
    if (!RectFromDict(*output_rect, &(pass->output_rect)))
      return nullptr;
  }

  if (ProcessRenderPassField(kRenderPassDamageRect)) {
    const base::Value::Dict* damage_rect = dict.FindDict("damage_rect");
    if (!damage_rect)
      return nullptr;
    if (!RectFromDict(*damage_rect, &(pass->damage_rect)))
      return nullptr;
  }

  if (ProcessRenderPassField(kRenderPassTransformToRootTarget)) {
    const base::Value::List* transform_to_root_target =
        dict.FindList("transform_to_root_target");
    if (!transform_to_root_target)
      return nullptr;
    if (!TransformFromList(*transform_to_root_target,
                           &(pass->transform_to_root_target))) {
      return nullptr;
    }
  }

  if (ProcessRenderPassField(kRenderPassFilters)) {
    const base::Value::List* filters = dict.FindList("filters");
    if (!filters)
      return nullptr;
    if (!FilterOperationsFromList(*filters, &(pass->filters)))
      return nullptr;
  }

  if (ProcessRenderPassField(kRenderPassBackdropFilters)) {
    const base::Value::List* backdrop_filters =
        dict.FindList("backdrop_filters");
    if (!backdrop_filters)
      return nullptr;
    if (!FilterOperationsFromList(*backdrop_filters,
                                  &(pass->backdrop_filters))) {
      return nullptr;
    }
  }

  if (ProcessRenderPassField(kRenderPassBackdropFilterBounds)) {
    const base::Value::Dict* backdrop_filter_bounds =
        dict.FindDict("backdrop_filter_bounds");
    if (backdrop_filter_bounds) {
      gfx::RRectF bounds;
      if (!RRectFFromDict(*backdrop_filter_bounds, &bounds))
        return nullptr;
      pass->backdrop_filter_bounds = bounds;
    }
  }

  if (ProcessRenderPassField(kRenderPassColorSpace)) {
    const base::Value::Dict* color_space = dict.FindDict("color_space");
    if (!color_space)
      return nullptr;

    // CompositorRenderPasses used to have a color space field, but this was
    // removed in favor of color usage. https://crbug.com/1049334
    gfx::ColorSpace pass_color_space = gfx::ColorSpace::CreateSRGB();
    if (!ColorSpaceFromDict(*color_space, &pass_color_space))
      return nullptr;
  }

  if (ProcessRenderPassField(kRenderPassHasTransparentBackground)) {
    const std::optional<bool> has_transparent_background =
        dict.FindBool("has_transparent_background");
    if (!has_transparent_background)
      return nullptr;
    pass->has_transparent_background = has_transparent_background.value();
  }

  if (ProcessRenderPassField(kRenderPassCacheRenderPass)) {
    const std::optional<bool> cache_render_pass =
        dict.FindBool("cache_render_pass");
    if (!cache_render_pass)
      return nullptr;
    pass->cache_render_pass = cache_render_pass.value();
  }

  if (ProcessRenderPassField(kRenderPassHasPreQuadDamage)) {
    const std::optional<bool> has_per_quad_damage =
        dict.FindBool("has_per_quad_damage");
    if (has_per_quad_damage)
      pass->has_per_quad_damage = has_per_quad_damage.value();
  }

  if (ProcessRenderPassField(kRenderPassHasDamageFromContributingContent)) {
    const std::optional<bool> has_damage_from_contributing_content =
        dict.FindBool("has_damage_from_contributing_content");
    if (!has_damage_from_contributing_content)
      return nullptr;
    pass->has_damage_from_contributing_content =
        has_damage_from_contributing_content.value();
  }

  if (ProcessRenderPassField(kRenderPassGenerateMipmap)) {
    const std::optional<bool> generate_mipmap =
        dict.FindBool("generate_mipmap");
    if (!generate_mipmap)
      return nullptr;
    pass->generate_mipmap = generate_mipmap.value();
  }

  if (ProcessRenderPassField(kRenderPassCopyRequests)) {
    // TODO(zmo): Read copy_requests.
  }

  // shared_quad_state_list has to be processed before quad_list.
  if (ProcessRenderPassField(kRenderPassSharedQuadStateList)) {
    const base::Value::List* shared_quad_state_list =
        dict.FindList("shared_quad_state_list");
    if (!shared_quad_state_list)
      return nullptr;
    if (!SharedQuadStateListFromList(*shared_quad_state_list,
                                     &(pass->shared_quad_state_list))) {
      return nullptr;
    }
  }

  if (ProcessRenderPassField(kRenderPassQuadList)) {
    const base::Value::List* quad_list = dict.FindList("quad_list");
    if (!quad_list)
      return nullptr;
    if (!QuadListFromList(*quad_list, &(pass->quad_list),
                          pass->shared_quad_state_list)) {
      return nullptr;
    }
  }

  return pass;
}

base::Value::Dict CompositorRenderPassListToDict(
    const CompositorRenderPassList& render_pass_list) {
  base::Value::List list;
  for (const auto& pass : render_pass_list) {
    list.Append(CompositorRenderPassToDict(*pass));
  }

  return base::Value::Dict()
      .Set("render_pass_count", static_cast<int>(render_pass_list.size()))
      .Set("metadata", GetRenderPassListMetadata(render_pass_list))
      .Set("render_pass_list", std::move(list));
}

bool CompositorRenderPassListFromDict(
    const base::Value::Dict& dict,
    CompositorRenderPassList* render_pass_list) {
  DCHECK(render_pass_list);
  DCHECK(render_pass_list->empty());

  const base::Value::List* list = dict.FindList("render_pass_list");
  if (!list) {
    return false;
  }

  for (const auto& item : *list) {
    const base::Value::Dict* item_dict = item.GetIfDict();
    if (!item_dict) {
      render_pass_list->clear();
      return false;
    }
    std::unique_ptr<CompositorRenderPass> render_pass =
        CompositorRenderPassFromDict(*item_dict);
    if (!render_pass) {
      render_pass_list->clear();
      return false;
    }
    render_pass_list->push_back(std::move(render_pass));
  }

  return true;
}

base::Value::Dict CompositorFrameToDict(
    const CompositorFrame& compositor_frame) {
  base::Value::List referenced_surfaces;
  for (auto& surface_range : compositor_frame.metadata.referenced_surfaces) {
    referenced_surfaces.Append(SurfaceRangeToDict(surface_range));
  }

  return base::Value::Dict()
      .Set("render_pass_list",
           CompositorRenderPassListToDict(compositor_frame.render_pass_list))
      .Set("metadata", base::Value::Dict().Set("referenced_surfaces",
                                               std::move(referenced_surfaces)));
}

bool CompositorFrameFromDict(const base::Value::Dict& dict,
                             CompositorFrame* compositor_frame) {
  DCHECK(compositor_frame);

  const base::Value::Dict* render_pass_list = dict.FindDict("render_pass_list");
  if (!render_pass_list) {
    return false;
  }
  if (!CompositorRenderPassListFromDict(*render_pass_list,
                                        &compositor_frame->render_pass_list)) {
    return false;
  }

  const base::Value::Dict* metadata = dict.FindDict("metadata");
  if (!metadata) {
    return false;
  }
  const base::Value::List* referenced_surfaces =
      metadata->FindList("referenced_surfaces");
  if (!referenced_surfaces) {
    return false;
  }
  for (auto& referenced_surface_dict : *referenced_surfaces) {
    auto referenced_surface =
        SurfaceRangeFromDict(referenced_surface_dict.GetDict());
    if (!referenced_surface) {
      return false;
    }
    compositor_frame->metadata.referenced_surfaces.push_back(
        *referenced_surface);
  }

  return true;
}

base::Value::List FrameDataToList(
    const std::vector<FrameData>& frame_data_list) {
  base::Value::List list;

  for (auto& frame_data : frame_data_list) {
    list.Append(
        base::Value::Dict()
            .Set("surface_id", SurfaceIdToDict(frame_data.surface_id))
            // This cast will be safe because we should never have more than
            // |INT_MAX| frames in recorded data.
            .Set("frame_index", static_cast<int>(frame_data.frame_index))
            .Set("compositor_frame",
                 CompositorFrameToDict(frame_data.compositor_frame)));
  }
  return list;
}

bool FrameDataFromList(const base::Value::List& list,
                       std::vector<FrameData>* frame_data_list) {
  DCHECK(frame_data_list);
  DCHECK(frame_data_list->empty());

  for (const auto& frame_data_value : list) {
    const base::Value::Dict* frame_data_dict = frame_data_value.GetIfDict();
    if (!frame_data_dict) {
      return false;
    }

    FrameData frame_data;
    const base::Value::Dict* surface_id_dict =
        frame_data_dict->FindDict("surface_id");
    if (!surface_id_dict) {
      return false;
    }
    std::optional<SurfaceId> surface_id = SurfaceIdFromDict(*surface_id_dict);
    if (!surface_id) {
      return false;
    }
    frame_data.surface_id = *surface_id;

    std::optional<int> frame_index = frame_data_dict->FindInt("frame_index");
    if (!frame_index) {
      return false;
    }
    frame_data.frame_index = *frame_index;

    const base::Value::Dict* compositor_frame_dict =
        frame_data_dict->FindDict("compositor_frame");
    if (!compositor_frame_dict) {
      return false;
    }
    if (!CompositorFrameFromDict(*compositor_frame_dict,
                                 &frame_data.compositor_frame)) {
      return false;
    }

    frame_data_list->push_back(std::move(frame_data));
  }

  return true;
}

FrameData::FrameData() = default;
FrameData::FrameData(FrameData&& other) = default;
FrameData& FrameData::operator=(FrameData&& other) = default;

FrameData::FrameData(const SurfaceId& surface_id,
                     const uint64_t frame_index,
                     const CompositorFrame& compositor_frame)
    : surface_id(surface_id), frame_index(frame_index) {
  this->compositor_frame.metadata = compositor_frame.metadata.Clone();
  this->compositor_frame.resource_list = compositor_frame.resource_list;
  for (const auto& render_pass : compositor_frame.render_pass_list) {
    this->compositor_frame.render_pass_list.push_back(render_pass->DeepCopy());
  }
}

}  // namespace viz
