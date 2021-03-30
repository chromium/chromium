// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/render_pass_io.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bit_cast.h"
#include "base/containers/span.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"

namespace gl {
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

base::Value RectToDict(const gfx::Rect& rect) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("x", rect.x());
  dict.SetIntKey("y", rect.y());
  dict.SetIntKey("width", rect.width());
  dict.SetIntKey("height", rect.height());
  return dict;
}

bool RectFromDict(const base::Value& dict, gfx::Rect* rect) {
  DCHECK(rect);
  if (!dict.is_dict())
    return false;
  base::Optional<int> x = dict.FindIntKey("x");
  base::Optional<int> y = dict.FindIntKey("y");
  base::Optional<int> width = dict.FindIntKey("width");
  base::Optional<int> height = dict.FindIntKey("height");
  if (!x || !y || !width || !height) {
    return false;
  }
  rect->SetRect(x.value(), y.value(), width.value(), height.value());
  return true;
}

base::Value RectFToDict(const gfx::RectF& rect) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetDoubleKey("x", rect.x());
  dict.SetDoubleKey("y", rect.y());
  dict.SetDoubleKey("width", rect.width());
  dict.SetDoubleKey("height", rect.height());
  return dict;
}

bool RectFFromDict(const base::Value& dict, gfx::RectF* rect) {
  DCHECK(rect);
  if (!dict.is_dict())
    return false;
  base::Optional<double> x = dict.FindDoubleKey("x");
  base::Optional<double> y = dict.FindDoubleKey("y");
  base::Optional<double> width = dict.FindDoubleKey("width");
  base::Optional<double> height = dict.FindDoubleKey("height");
  if (!x || !y || !width || !height) {
    return false;
  }
  rect->SetRect(static_cast<float>(x.value()), static_cast<float>(y.value()),
                static_cast<float>(width.value()),
                static_cast<float>(height.value()));
  return true;
}

base::Value SizeToDict(const gfx::Size& size) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("width", size.width());
  dict.SetIntKey("height", size.height());
  return dict;
}

bool SizeFromDict(const base::Value& dict, gfx::Size* size) {
  DCHECK(size);
  if (!dict.is_dict())
    return false;
  base::Optional<int> width = dict.FindIntKey("width");
  base::Optional<int> height = dict.FindIntKey("height");
  if (!width || !height) {
    return false;
  }
  size->set_width(width.value());
  size->set_height(height.value());
  return true;
}

base::Value PointToDict(const gfx::Point& point) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("x", point.x());
  dict.SetIntKey("y", point.y());
  return dict;
}

bool PointFromDict(const base::Value& dict, gfx::Point* point) {
  DCHECK(point);
  if (!dict.is_dict())
    return false;
  base::Optional<int> x = dict.FindIntKey("x");
  base::Optional<int> y = dict.FindIntKey("y");
  if (!x || !y) {
    return false;
  }
  point->set_x(x.value());
  point->set_y(y.value());
  return true;
}

base::Value PointFToDict(const gfx::PointF& point) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetDoubleKey("x", point.x());
  dict.SetDoubleKey("y", point.y());
  return dict;
}

bool PointFFromDict(const base::Value& dict, gfx::PointF* point) {
  DCHECK(point);
  if (!dict.is_dict())
    return false;
  base::Optional<double> x = dict.FindDoubleKey("x");
  base::Optional<double> y = dict.FindDoubleKey("y");
  if (!x || !y) {
    return false;
  }
  point->set_x(static_cast<float>(x.value()));
  point->set_y(static_cast<float>(y.value()));
  return true;
}

base::Value Vector2dFToDict(const gfx::Vector2dF& v) {
  return PointFToDict(gfx::PointF(v.x(), v.y()));
}

bool Vector2dFFromDict(const base::Value& dict, gfx::Vector2dF* v) {
  DCHECK(v);
  gfx::PointF point;
  if (!PointFFromDict(dict, &point))
    return false;
  v->set_x(point.x());
  v->set_y(point.y());
  return true;
}

base::Value FloatArrayToList(base::span<const float> data) {
  base::Value list(base::Value::Type::LIST);
  for (float num : data)
    list.Append(num);
  return list;
}

bool FloatArrayFromList(const base::Value& list,
                        size_t expected_count,
                        float* data) {
  DCHECK(data);
  DCHECK_LT(0u, expected_count);
  if (!list.is_list())
    return false;
  size_t count = list.GetList().size();
  if (count != expected_count)
    return false;
  std::vector<double> double_data(count);
  for (size_t ii = 0; ii < count; ++ii) {
    if (!list.GetList()[ii].is_double())
      return false;
    double_data[ii] = list.GetList()[ii].GetDouble();
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
      NOTREACHED();
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

base::Value RRectFToDict(const gfx::RRectF& rect) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("type", RRectFTypeToString(rect.GetType()));
  if (rect.GetType() != gfx::RRectF::Type::kEmpty) {
    dict.SetKey("rect", RectFToDict(rect.rect()));
    dict.SetDoubleKey("upper_left.x",
                      rect.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).x());
    dict.SetDoubleKey("upper_left.y",
                      rect.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).y());
    dict.SetDoubleKey(
        "upper_right.x",
        rect.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).x());
    dict.SetDoubleKey(
        "upper_right.y",
        rect.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).y());
    dict.SetDoubleKey(
        "lower_right.x",
        rect.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).x());
    dict.SetDoubleKey(
        "lower_right.y",
        rect.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).y());
    dict.SetDoubleKey("lower_left.x",
                      rect.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).x());
    dict.SetDoubleKey("lower_left.y",
                      rect.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).y());
  }
  return dict;
}

bool RRectFFromDict(const base::Value& dict, gfx::RRectF* out) {
  DCHECK(out);
  if (!dict.is_dict())
    return false;
  const std::string* type = dict.FindStringKey("type");
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
  const base::Value* rect = dict.FindDictKey("rect");
  base::Optional<double> upper_left_x = dict.FindDoubleKey("upper_left.x");
  base::Optional<double> upper_left_y = dict.FindDoubleKey("upper_left.y");
  base::Optional<double> upper_right_x = dict.FindDoubleKey("upper_right.x");
  base::Optional<double> upper_right_y = dict.FindDoubleKey("upper_right.y");
  base::Optional<double> lower_right_x = dict.FindDoubleKey("lower_right.x");
  base::Optional<double> lower_right_y = dict.FindDoubleKey("lower_right.y");
  base::Optional<double> lower_left_x = dict.FindDoubleKey("lower_left.x");
  base::Optional<double> lower_left_y = dict.FindDoubleKey("lower_left.y");
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

base::Value TransformToList(const gfx::Transform& transform) {
  base::Value list(base::Value::Type::LIST);
  double data[16];
  transform.matrix().asColMajord(data);
  for (size_t ii = 0; ii < 16; ++ii)
    list.Append(data[ii]);
  return list;
}

bool TransformFromList(const base::Value& list, gfx::Transform* transform) {
  DCHECK(transform);
  if (!list.is_list())
    return false;
  if (list.GetList().size() != 16)
    return false;
  double data[16];
  for (size_t ii = 0; ii < 16; ++ii) {
    if (!list.GetList()[ii].is_double())
      return false;
    data[ii] = list.GetList()[ii].GetDouble();
  }
  transform->matrix().setColMajord(data);
  return true;
}

base::Value ShapeRectsToList(const cc::FilterOperation::ShapeRects& shape) {
  base::Value list(base::Value::Type::LIST);
  for (size_t ii = 0; ii < shape.size(); ++ii) {
    list.Append(RectToDict(shape[ii]));
  }
  return list;
}

bool ShapeRectsFromList(const base::Value& list,
                        cc::FilterOperation::ShapeRects* shape) {
  DCHECK(shape);
  if (!list.is_list())
    return false;
  size_t size = list.GetList().size();
  cc::FilterOperation::ShapeRects data;
  data.resize(size);
  for (size_t ii = 0; ii < size; ++ii) {
    if (!RectFromDict(list.GetList()[ii], &data[ii]))
      return false;
  }
  *shape = data;
  return true;
}

std::string PaintFilterToString(const sk_sp<cc::PaintFilter>& filter) {
  // TODO(zmo): Expand to readable fields. Such recorded data becomes invalid
  // when we update any data structure.
  std::vector<uint8_t> buffer(cc::PaintOpWriter::HeaderBytes() +
                              cc::PaintFilter::GetFilterSize(filter.get()));
  // No need to populate the SerializeOptions here since the security
  // constraints explicitly disable serializing images using the transfer cache
  // and serialization of PaintRecords.
  cc::PaintOp::SerializeOptions options(nullptr, nullptr, nullptr, nullptr,
                                        nullptr, nullptr, false, false, 0,
                                        SkM44());
  cc::PaintOpWriter writer(buffer.data(), buffer.size(), options,
                           true /* enable_security_constraints */);
  writer.Write(filter.get());
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
  cc::PaintOp::DeserializeOptions options(nullptr, nullptr, nullptr,
                                          &scratch_buffer, false, nullptr);
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

base::Value FilterOperationToDict(const cc::FilterOperation& filter) {
  base::Value dict(base::Value::Type::DICTIONARY);
  cc::FilterOperation::FilterType type = filter.type();

  dict.SetIntKey("type", type);
  if (type != cc::FilterOperation::COLOR_MATRIX &&
      type != cc::FilterOperation::REFERENCE) {
    dict.SetDoubleKey("amount", filter.amount());
  }
  switch (type) {
    case cc::FilterOperation::ALPHA_THRESHOLD:
      dict.SetDoubleKey("outer_threshold", filter.outer_threshold());
      dict.SetKey("shape", ShapeRectsToList(filter.shape()));
      break;
    case cc::FilterOperation::DROP_SHADOW:
      dict.SetKey("drop_shadow_offset",
                  PointToDict(filter.drop_shadow_offset()));
      dict.SetIntKey("drop_shadow_color",
                     bit_cast<int>(filter.drop_shadow_color()));
      break;
    case cc::FilterOperation::REFERENCE:
      dict.SetStringKey("image_filter",
                        PaintFilterToString(filter.image_filter()));
      break;
    case cc::FilterOperation::COLOR_MATRIX:
      dict.SetKey("matrix", FloatArrayToList(filter.matrix()));
      break;
    case cc::FilterOperation::ZOOM:
      dict.SetIntKey("zoom_inset", filter.zoom_inset());
      break;
    case cc::FilterOperation::BLUR:
      dict.SetIntKey("blur_tile_mode",
                     static_cast<int>(filter.blur_tile_mode()));
      break;
    default:
      break;
  }
  return dict;
}

bool FilterOperationFromDict(const base::Value& dict,
                             cc::FilterOperation* out) {
  DCHECK(out);
  if (!dict.is_dict()) {
    return false;
  }

  base::Optional<int> type = dict.FindIntKey("type");
  base::Optional<double> amount = dict.FindDoubleKey("amount");
  base::Optional<double> outer_threshold =
      dict.FindDoubleKey("outer_threshold");
  const base::Value* drop_shadow_offset =
      dict.FindDictKey("drop_shadow_offset");
  base::Optional<int> drop_shadow_color = dict.FindIntKey("drop_shadow_color");
  const std::string* image_filter = dict.FindStringKey("image_filter");
  const base::Value* matrix = dict.FindListKey("matrix");
  base::Optional<int> zoom_inset = dict.FindIntKey("zoom_inset");
  const base::Value* shape = dict.FindListKey("shape");
  base::Optional<int> blur_tile_mode = dict.FindIntKey("blur_tile_mode");

  cc::FilterOperation filter;

  if (!type)
    return false;
  cc::FilterOperation::FilterType filter_type =
      static_cast<cc::FilterOperation::FilterType>(type.value());
  filter.set_type(filter_type);
  if (filter_type != cc::FilterOperation::COLOR_MATRIX &&
      filter_type != cc::FilterOperation::REFERENCE) {
    if (!amount)
      return false;
    filter.set_amount(static_cast<float>(amount.value()));
  }
  switch (filter_type) {
    case cc::FilterOperation::ALPHA_THRESHOLD: {
      cc::FilterOperation::ShapeRects shape_rects;
      if (!outer_threshold || !shape ||
          !ShapeRectsFromList(*shape, &shape_rects)) {
        return false;
      }
      filter.set_outer_threshold(static_cast<float>(outer_threshold.value()));
      filter.set_shape(shape_rects);
    } break;
    case cc::FilterOperation::DROP_SHADOW: {
      gfx::Point offset;
      if (!drop_shadow_offset || !drop_shadow_color ||
          !PointFromDict(*drop_shadow_offset, &offset)) {
        return false;
      }
      filter.set_drop_shadow_offset(offset);
      filter.set_drop_shadow_color(
          bit_cast<SkColor>(drop_shadow_color.value()));
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
    default:
      break;
  }

  *out = filter;
  return true;
}

base::Value FilterOperationsToList(const cc::FilterOperations& filters) {
  base::Value list(base::Value::Type::LIST);
  for (size_t ii = 0; ii < filters.size(); ++ii) {
    base::Value filter_dict = FilterOperationToDict(filters.at(ii));
    list.Append(std::move(filter_dict));
  }
  return list;
}

bool FilterOperationsFromList(const base::Value& list,
                              cc::FilterOperations* filters) {
  DCHECK(filters);
  if (!list.is_list())
    return false;
  cc::FilterOperations data;
  for (size_t ii = 0; ii < list.GetList().size(); ++ii) {
    cc::FilterOperation filter;
    if (!FilterOperationFromDict(list.GetList()[ii], &filter))
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
    MATCH_ENUM_CASE(PrimaryID, SMPTEST432_1)
    MATCH_ENUM_CASE(PrimaryID, XYZ_D50)
    MATCH_ENUM_CASE(PrimaryID, ADOBE_RGB)
    MATCH_ENUM_CASE(PrimaryID, APPLE_GENERIC_RGB)
    MATCH_ENUM_CASE(PrimaryID, WIDE_GAMUT_COLOR_SPIN)
    MATCH_ENUM_CASE(PrimaryID, CUSTOM)
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
    MATCH_ENUM_CASE(TransferID, IEC61966_2_1)
    MATCH_ENUM_CASE(TransferID, BT2020_10)
    MATCH_ENUM_CASE(TransferID, BT2020_12)
    MATCH_ENUM_CASE(TransferID, SMPTEST2084)
    MATCH_ENUM_CASE(TransferID, SMPTEST428_1)
    MATCH_ENUM_CASE(TransferID, ARIB_STD_B67)
    MATCH_ENUM_CASE(TransferID, IEC61966_2_1_HDR)
    MATCH_ENUM_CASE(TransferID, LINEAR_HDR)
    MATCH_ENUM_CASE(TransferID, CUSTOM)
    MATCH_ENUM_CASE(TransferID, CUSTOM_HDR)
    MATCH_ENUM_CASE(TransferID, PIECEWISE_HDR)
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
    MATCH_ENUM_CASE(MatrixID, BT2020_CL)
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
  MATCH_ENUM_CASE(PrimaryID, SMPTEST432_1)
  MATCH_ENUM_CASE(PrimaryID, XYZ_D50)
  MATCH_ENUM_CASE(PrimaryID, ADOBE_RGB)
  MATCH_ENUM_CASE(PrimaryID, APPLE_GENERIC_RGB)
  MATCH_ENUM_CASE(PrimaryID, WIDE_GAMUT_COLOR_SPIN)
  MATCH_ENUM_CASE(PrimaryID, CUSTOM)
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
  MATCH_ENUM_CASE(TransferID, IEC61966_2_1)
  MATCH_ENUM_CASE(TransferID, BT2020_10)
  MATCH_ENUM_CASE(TransferID, BT2020_12)
  MATCH_ENUM_CASE(TransferID, SMPTEST2084)
  MATCH_ENUM_CASE(TransferID, SMPTEST428_1)
  MATCH_ENUM_CASE(TransferID, ARIB_STD_B67)
  MATCH_ENUM_CASE(TransferID, IEC61966_2_1_HDR)
  MATCH_ENUM_CASE(TransferID, LINEAR_HDR)
  MATCH_ENUM_CASE(TransferID, CUSTOM)
  MATCH_ENUM_CASE(TransferID, CUSTOM_HDR)
  MATCH_ENUM_CASE(TransferID, PIECEWISE_HDR)
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
  MATCH_ENUM_CASE(MatrixID, BT2020_CL)
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

base::Value Matrix3x3ToList(const skcms_Matrix3x3& mat) {
  float data[9];
  memcpy(data, mat.vals, sizeof(mat));
  return FloatArrayToList(data);
}

bool Matrix3x3FromList(const base::Value& list, skcms_Matrix3x3* mat) {
  DCHECK(mat);
  return FloatArrayFromList(list, 9u, reinterpret_cast<float*>(mat->vals));
}

base::Value TransferFunctionToList(const skcms_TransferFunction& fn) {
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

bool TransferFunctionFromList(const base::Value& list,
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

base::Value ColorSpaceToDict(const gfx::ColorSpace& color_space) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("primaries",
                    ColorSpacePrimaryIdToString(color_space.GetPrimaryID()));
  dict.SetStringKey("transfer",
                    ColorSpaceTransferIdToString(color_space.GetTransferID()));
  dict.SetStringKey("matrix",
                    ColorSpaceMatrixIdToString(color_space.GetMatrixID()));
  dict.SetStringKey("range",
                    ColorSpaceRangeIdToString(color_space.GetRangeID()));
  if (color_space.GetPrimaryID() == gfx::ColorSpace::PrimaryID::CUSTOM) {
    skcms_Matrix3x3 mat;
    color_space.GetPrimaryMatrix(&mat);
    dict.SetKey("custom_primary_matrix", Matrix3x3ToList(mat));
  }
  if (color_space.GetTransferID() == gfx::ColorSpace::TransferID::CUSTOM ||
      color_space.GetTransferID() == gfx::ColorSpace::TransferID::CUSTOM_HDR) {
    skcms_TransferFunction fn;
    color_space.GetTransferFunction(&fn);
    dict.SetKey("custom_transfer_params", TransferFunctionToList(fn));
  }
  return dict;
}

bool ColorSpaceFromDict(const base::Value& dict, gfx::ColorSpace* color_space) {
  DCHECK(color_space);
  if (!dict.is_dict())
    return false;
  const std::string* primaries = dict.FindStringKey("primaries");
  const std::string* transfer = dict.FindStringKey("transfer");
  const std::string* matrix = dict.FindStringKey("matrix");
  const std::string* range = dict.FindStringKey("range");
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
    const base::Value* custom_primary_matrix =
        dict.FindListKey("custom_primary_matrix");
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
    const base::Value* custom_transfer_params =
        dict.FindListKey("custom_transfer_params");
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

base::Value DrawQuadResourcesToList(const DrawQuad::Resources& resources) {
  base::Value list(base::Value::Type::LIST);
  DCHECK_LE(resources.count, DrawQuad::Resources::kMaxResourceIdCount);
  for (ResourceId id : resources)
    list.Append(static_cast<int>(id.GetUnsafeValue()));
  return list;
}

bool DrawQuadResourcesFromList(const base::Value& list,
                               DrawQuad::Resources* resources) {
  DCHECK(resources);
  if (!list.is_list())
    return false;
  size_t size = list.GetList().size();
  if (size == 0u) {
    resources->count = 0u;
    return true;
  }
  if (size > DrawQuad::Resources::kMaxResourceIdCount)
    return false;
  for (size_t ii = 0; ii < size; ++ii) {
    if (!list.GetList()[ii].is_int())
      return false;
  }

  resources->count = static_cast<uint32_t>(size);
  for (size_t ii = 0; ii < size; ++ii) {
    resources->ids[ii] = ResourceId(list.GetList()[ii].GetInt());
  }
  return true;
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

#define MAP_MATERIAL_TO_STRING(NAME) \
  case DrawQuad::Material::NAME:     \
    return #NAME;
const char* DrawQuadMaterialToString(DrawQuad::Material material) {
  switch (material) {
    MAP_MATERIAL_TO_STRING(kInvalid)
    MAP_MATERIAL_TO_STRING(kDebugBorder)
    MAP_MATERIAL_TO_STRING(kPictureContent)
    MAP_MATERIAL_TO_STRING(kCompositorRenderPass)
    MAP_MATERIAL_TO_STRING(kSolidColor)
    MAP_MATERIAL_TO_STRING(kStreamVideoContent)
    MAP_MATERIAL_TO_STRING(kSurfaceContent)
    MAP_MATERIAL_TO_STRING(kTextureContent)
    MAP_MATERIAL_TO_STRING(kTiledContent)
    MAP_MATERIAL_TO_STRING(kYuvVideoContent)
    MAP_MATERIAL_TO_STRING(kVideoHole)
    default:
      NOTREACHED();
      return "";
  }
}
#undef MAP_MATERIAL_TO_STRING

#define MAP_STRING_TO_MATERIAL(NAME) \
  if (str == #NAME)                  \
    return static_cast<int>(DrawQuad::Material::NAME);
int StringToDrawQuadMaterial(const std::string& str) {
  MAP_STRING_TO_MATERIAL(kInvalid)
  MAP_STRING_TO_MATERIAL(kDebugBorder)
  MAP_STRING_TO_MATERIAL(kPictureContent)
  MAP_STRING_TO_MATERIAL(kCompositorRenderPass)
  MAP_STRING_TO_MATERIAL(kSolidColor)
  MAP_STRING_TO_MATERIAL(kStreamVideoContent)
  MAP_STRING_TO_MATERIAL(kSurfaceContent)
  MAP_STRING_TO_MATERIAL(kTextureContent)
  MAP_STRING_TO_MATERIAL(kTiledContent)
  MAP_STRING_TO_MATERIAL(kYuvVideoContent)
  MAP_STRING_TO_MATERIAL(kVideoHole)
  return -1;
}
#undef MAP_STRING_TO_MATERIAL

void DrawQuadCommonToDict(const DrawQuad* draw_quad,
                          base::Value* dict,
                          const SharedQuadStateList& shared_quad_state_list) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->SetStringKey("material", DrawQuadMaterialToString(draw_quad->material));
  dict->SetKey("rect", RectToDict(draw_quad->rect));
  dict->SetKey("visible_rect", RectToDict(draw_quad->visible_rect));
  dict->SetBoolKey("needs_blending", draw_quad->needs_blending);
  int shared_quad_state_index = GetSharedQuadStateIndex(
      shared_quad_state_list, draw_quad->shared_quad_state);
  DCHECK_LE(0, shared_quad_state_index);
  dict->SetIntKey("shared_quad_state_index", shared_quad_state_index);
  dict->SetKey("resources", DrawQuadResourcesToList(draw_quad->resources));
}

void ContentDrawQuadCommonToDict(const ContentDrawQuadBase* draw_quad,
                                 base::Value* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->SetKey("tex_coord_rect", RectFToDict(draw_quad->tex_coord_rect));
  dict->SetKey("texture_size", SizeToDict(draw_quad->texture_size));
  dict->SetBoolKey("is_premultiplied", draw_quad->is_premultiplied);
  dict->SetBoolKey("nearest_neighbor", draw_quad->nearest_neighbor);
  dict->SetBoolKey("force_anti_aliasing_off",
                   draw_quad->force_anti_aliasing_off);
}

struct DrawQuadCommon {
  DrawQuad::Material material = DrawQuad::Material::kInvalid;
  gfx::Rect rect;
  gfx::Rect visible_rect;
  bool needs_blending = false;
  const SharedQuadState* shared_quad_state = nullptr;
  DrawQuad::Resources resources;
};

base::Optional<DrawQuadCommon> GetDrawQuadCommonFromDict(
    const base::Value& dict,
    const SharedQuadStateList& shared_quad_state_list) {
  if (!dict.is_dict())
    return base::nullopt;
  const std::string* material = dict.FindStringKey("material");
  const base::Value* rect = dict.FindDictKey("rect");
  const base::Value* visible_rect = dict.FindDictKey("visible_rect");
  base::Optional<bool> needs_blending = dict.FindBoolKey("needs_blending");
  base::Optional<int> shared_quad_state_index =
      dict.FindIntKey("shared_quad_state_index");
  const base::Value* resources = dict.FindListKey("resources");
  if (!material || !rect || !visible_rect || !needs_blending ||
      !shared_quad_state_index || !resources) {
    return base::nullopt;
  }
  int material_index = StringToDrawQuadMaterial(*material);
  if (material_index < 0)
    return base::nullopt;
  int sqs_index = shared_quad_state_index.value();
  if (sqs_index < 0 ||
      static_cast<size_t>(sqs_index) >= shared_quad_state_list.size()) {
    return base::nullopt;
  }
  gfx::Rect t_rect, t_visible_rect;
  if (!RectFromDict(*rect, &t_rect) ||
      !RectFromDict(*visible_rect, &t_visible_rect)) {
    return base::nullopt;
  }
  DrawQuad::Resources t_resources;
  if (!DrawQuadResourcesFromList(*resources, &t_resources))
    return base::nullopt;

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

base::Optional<ContentDrawQuadCommon> GetContentDrawQuadCommonFromDict(
    const base::Value& dict) {
  if (!dict.is_dict())
    return base::nullopt;
  const base::Value* tex_coord_rect = dict.FindDictKey("tex_coord_rect");
  const base::Value* texture_size = dict.FindDictKey("texture_size");
  base::Optional<bool> is_premultiplied = dict.FindBoolKey("is_premultiplied");
  base::Optional<bool> nearest_neighbor = dict.FindBoolKey("nearest_neighbor");
  base::Optional<bool> force_anti_aliasing_off =
      dict.FindBoolKey("force_anti_aliasing_off");

  if (!tex_coord_rect || !texture_size || !is_premultiplied ||
      !nearest_neighbor || !force_anti_aliasing_off) {
    return base::nullopt;
  }
  gfx::RectF t_tex_coord_rect;
  gfx::Size t_texture_size;
  if (!RectFFromDict(*tex_coord_rect, &t_tex_coord_rect) ||
      !SizeFromDict(*texture_size, &t_texture_size)) {
    return base::nullopt;
  }

  return ContentDrawQuadCommon{
      t_tex_coord_rect, t_texture_size, is_premultiplied.value(),
      nearest_neighbor.value(), force_anti_aliasing_off.value()};
}

void CompositorRenderPassDrawQuadToDict(
    const CompositorRenderPassDrawQuad* draw_quad,
    base::Value* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->SetStringKey(
      "render_pass_id",
      base::NumberToString(static_cast<uint64_t>(draw_quad->render_pass_id)));
  dict->SetKey("mask_uv_rect", RectFToDict(draw_quad->mask_uv_rect));
  dict->SetKey("mask_texture_size", SizeToDict(draw_quad->mask_texture_size));
  dict->SetKey("filters_scale", Vector2dFToDict(draw_quad->filters_scale));
  dict->SetKey("filters_origin", PointFToDict(draw_quad->filters_origin));
  dict->SetKey("tex_coord_rect", RectFToDict(draw_quad->tex_coord_rect));
  dict->SetDoubleKey("backdrop_filter_quality",
                     draw_quad->backdrop_filter_quality);
  dict->SetBoolKey("force_anti_aliasing_off",
                   draw_quad->force_anti_aliasing_off);
  dict->SetBoolKey("intersects_damage_under",
                   draw_quad->intersects_damage_under);
  DCHECK_GE(1u, draw_quad->resources.count);
}

void SolidColorDrawQuadToDict(const SolidColorDrawQuad* draw_quad,
                              base::Value* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->SetIntKey("color", static_cast<int>(draw_quad->color));
  dict->SetBoolKey("force_anti_aliasing_off",
                   draw_quad->force_anti_aliasing_off);
}

void StreamVideoDrawQuadToDict(const StreamVideoDrawQuad* draw_quad,
                               base::Value* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->SetKey("uv_top_left", PointFToDict(draw_quad->uv_top_left));
  dict->SetKey("uv_bottom_right", PointFToDict(draw_quad->uv_bottom_right));
  DCHECK_EQ(1u, draw_quad->resources.count);
  dict->SetKey("overlay_resource_size_in_pixels",
               SizeToDict(draw_quad->overlay_resources.size_in_pixels));
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
      NOTREACHED();
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

void TextureDrawQuadToDict(const TextureDrawQuad* draw_quad,
                           base::Value* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->SetBoolKey("premultiplied_alpha", draw_quad->premultiplied_alpha);
  dict->SetKey("uv_top_left", PointFToDict(draw_quad->uv_top_left));
  dict->SetKey("uv_bottom_right", PointFToDict(draw_quad->uv_bottom_right));
  dict->SetIntKey("background_color",
                  static_cast<int>(draw_quad->background_color));
  dict->SetKey("vertex_opacity", FloatArrayToList(draw_quad->vertex_opacity));
  dict->SetBoolKey("y_flipped", draw_quad->y_flipped);
  dict->SetBoolKey("nearest_neighbor", draw_quad->nearest_neighbor);
  dict->SetBoolKey("secure_output_only", draw_quad->secure_output_only);
  dict->SetStringKey(
      "protected_video_type",
      ProtectedVideoTypeToString(draw_quad->protected_video_type));
  DCHECK_EQ(1u, draw_quad->resources.count);
  dict->SetKey("resource_size_in_pixels",
               SizeToDict(draw_quad->overlay_resources.size_in_pixels));
}

void TileDrawQuadToDict(const TileDrawQuad* draw_quad, base::Value* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  ContentDrawQuadCommonToDict(draw_quad, dict);
  DCHECK_EQ(1u, draw_quad->resources.count);
}

void YUVVideoDrawQuadToDict(const YUVVideoDrawQuad* draw_quad,
                            base::Value* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->SetKey("ya_tex_coord_rect", RectFToDict(draw_quad->ya_tex_coord_rect));
  dict->SetKey("uv_tex_coord_rect", RectFToDict(draw_quad->uv_tex_coord_rect));
  dict->SetKey("ya_tex_size", SizeToDict(draw_quad->ya_tex_size));
  dict->SetKey("uv_tex_size", SizeToDict(draw_quad->uv_tex_size));
  dict->SetDoubleKey("resource_offset", draw_quad->resource_offset);
  dict->SetDoubleKey("resource_multiplier", draw_quad->resource_multiplier);
  dict->SetIntKey("bits_per_channel", draw_quad->bits_per_channel);
  dict->SetKey("video_color_space",
               ColorSpaceToDict(draw_quad->video_color_space));
  dict->SetStringKey(
      "protected_video_type",
      ProtectedVideoTypeToString(draw_quad->protected_video_type));
  DCHECK(4u == draw_quad->resources.count || 3u == draw_quad->resources.count);
}

void VideoHoleDrawQuadToDict(const VideoHoleDrawQuad* draw_quad,
                             base::Value* dict) {
  DCHECK(draw_quad);
  DCHECK(dict);
  dict->SetBoolKey("overlay_plane_id.empty",
                   draw_quad->overlay_plane_id.is_empty());
  if (!draw_quad->overlay_plane_id.is_empty()) {
    uint64_t high = draw_quad->overlay_plane_id.GetHighForSerialization();
    dict->SetStringKey("overlay_plane_id.high", base::NumberToString(high));
    uint64_t low = draw_quad->overlay_plane_id.GetLowForSerialization();
    dict->SetStringKey("overlay_plane_id.low", base::NumberToString(low));
  }
}

#define UNEXPECTED_DRAW_QUAD_TYPE(NAME)     \
  case DrawQuad::Material::NAME:            \
    NOTREACHED() << "Unexpected " << #NAME; \
    break;
#define WRITE_DRAW_QUAD_TYPE_FIELDS(NAME, TYPE)                    \
  case DrawQuad::Material::NAME:                                   \
    TYPE##ToDict(reinterpret_cast<const TYPE*>(draw_quad), &dict); \
    break;
base::Value DrawQuadToDict(const DrawQuad* draw_quad,
                           const SharedQuadStateList& shared_quad_state_list) {
  DCHECK(draw_quad);
  base::Value dict(base::Value::Type::DICTIONARY);
  DrawQuadCommonToDict(draw_quad, &dict, shared_quad_state_list);
  switch (draw_quad->material) {
    WRITE_DRAW_QUAD_TYPE_FIELDS(kCompositorRenderPass,
                                CompositorRenderPassDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kSolidColor, SolidColorDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kStreamVideoContent, StreamVideoDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kTextureContent, TextureDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kTiledContent, TileDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kYuvVideoContent, YUVVideoDrawQuad)
    WRITE_DRAW_QUAD_TYPE_FIELDS(kVideoHole, VideoHoleDrawQuad)
    UNEXPECTED_DRAW_QUAD_TYPE(kPictureContent)
    UNEXPECTED_DRAW_QUAD_TYPE(kSurfaceContent)
    default:
      break;
  }
  return dict;
}
#undef WRITE_DRAW_QUAD_TYPE_FIELDS
#undef UNEXPECTED_DRAW_QUAD_TYPE

base::Value QuadListToList(const QuadList& quad_list,
                           const SharedQuadStateList& shared_quad_state_list) {
  base::Value list(base::Value::Type::LIST);
  for (size_t ii = 0; ii < quad_list.size(); ++ii) {
    list.Append(
        DrawQuadToDict(quad_list.ElementAt(ii), shared_quad_state_list));
  }
  return list;
}

bool CompositorRenderPassDrawQuadFromDict(
    const base::Value& dict,
    const DrawQuadCommon& common,
    CompositorRenderPassDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  if (!dict.is_dict())
    return false;
  if (common.resources.count > 1u)
    return false;
  const std::string* render_pass_id = dict.FindStringKey("render_pass_id");
  const base::Value* mask_uv_rect = dict.FindDictKey("mask_uv_rect");
  const base::Value* mask_texture_size = dict.FindDictKey("mask_texture_size");
  const base::Value* filters_scale = dict.FindDictKey("filters_scale");
  const base::Value* filters_origin = dict.FindDictKey("filters_origin");
  const base::Value* tex_coord_rect = dict.FindDictKey("tex_coord_rect");
  base::Optional<double> backdrop_filter_quality =
      dict.FindDoubleKey("backdrop_filter_quality");
  base::Optional<bool> force_anti_aliasing_off =
      dict.FindBoolKey("force_anti_aliasing_off");
  base::Optional<bool> intersects_damage_under =
      dict.FindBoolKey("intersects_damage_under");

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

bool SolidColorDrawQuadFromDict(const base::Value& dict,
                                const DrawQuadCommon& common,
                                SolidColorDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  if (!dict.is_dict())
    return false;
  base::Optional<int> color = dict.FindIntKey("color");
  base::Optional<bool> force_anti_aliasing_off =
      dict.FindBoolKey("force_anti_aliasing_off");
  if (!color || !force_anti_aliasing_off)
    return false;
  draw_quad->SetAll(common.shared_quad_state, common.rect, common.visible_rect,
                    common.needs_blending, static_cast<SkColor>(color.value()),
                    force_anti_aliasing_off.value());
  return true;
}

bool StreamVideoDrawQuadFromDict(const base::Value& dict,
                                 const DrawQuadCommon& common,
                                 StreamVideoDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  if (!dict.is_dict())
    return false;
  if (common.resources.count != 1u)
    return false;
  const base::Value* uv_top_left = dict.FindDictKey("uv_top_left");
  const base::Value* uv_bottom_right = dict.FindDictKey("uv_bottom_right");
  const base::Value* overlay_resource_size_in_pixels =
      dict.FindDictKey("overlay_resource_size_in_pixels");

  if (!uv_top_left || !uv_bottom_right || !overlay_resource_size_in_pixels) {
    return false;
  }
  gfx::PointF t_uv_top_left, t_uv_bottom_right;
  gfx::Size t_overlay_resource_size_in_pixels;
  if (!PointFFromDict(*uv_top_left, &t_uv_top_left) ||
      !PointFFromDict(*uv_bottom_right, &t_uv_bottom_right) ||
      !SizeFromDict(*overlay_resource_size_in_pixels,
                    &t_overlay_resource_size_in_pixels)) {
    return false;
  }

  const size_t kIndex = StreamVideoDrawQuad::kResourceIdIndex;
  ResourceId resource_id = common.resources.ids[kIndex];

  draw_quad->SetAll(common.shared_quad_state, common.rect, common.visible_rect,
                    common.needs_blending, resource_id,
                    t_overlay_resource_size_in_pixels, t_uv_top_left,
                    t_uv_bottom_right);
  return true;
}

bool TextureDrawQuadFromDict(const base::Value& dict,
                             const DrawQuadCommon& common,
                             TextureDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  if (!dict.is_dict())
    return false;
  if (common.resources.count != 1u)
    return false;

  base::Optional<bool> premultiplied_alpha =
      dict.FindBoolKey("premultiplied_alpha");
  const base::Value* uv_top_left = dict.FindDictKey("uv_top_left");
  const base::Value* uv_bottom_right = dict.FindDictKey("uv_bottom_right");
  base::Optional<int> background_color = dict.FindIntKey("background_color");
  const base::Value* vertex_opacity = dict.FindListKey("vertex_opacity");
  base::Optional<bool> y_flipped = dict.FindBoolKey("y_flipped");
  base::Optional<bool> nearest_neighbor = dict.FindBoolKey("nearest_neighbor");
  base::Optional<bool> secure_output_only =
      dict.FindBoolKey("secure_output_only");
  const std::string* protected_video_type =
      dict.FindStringKey("protected_video_type");
  const base::Value* resource_size_in_pixels =
      dict.FindDictKey("resource_size_in_pixels");

  if (!premultiplied_alpha || !uv_top_left || !uv_bottom_right ||
      !background_color || !vertex_opacity || !y_flipped || !nearest_neighbor ||
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
  if (!PointFFromDict(*uv_top_left, &t_uv_top_left) ||
      !PointFFromDict(*uv_bottom_right, &t_uv_bottom_right) ||
      !SizeFromDict(*resource_size_in_pixels, &t_resource_size_in_pixels)) {
    return false;
  }
  float t_vertex_opacity[4];
  if (!FloatArrayFromList(*vertex_opacity, 4u, t_vertex_opacity))
    return false;
  const size_t kIndex = TextureDrawQuad::kResourceIdIndex;
  ResourceId resource_id = common.resources.ids[kIndex];
  draw_quad->SetAll(
      common.shared_quad_state, common.rect, common.visible_rect,
      common.needs_blending, resource_id, t_resource_size_in_pixels,
      premultiplied_alpha.value(), t_uv_top_left, t_uv_bottom_right,
      static_cast<SkColor>(background_color.value()), t_vertex_opacity,
      y_flipped.value(), nearest_neighbor.value(), secure_output_only.value(),
      static_cast<gfx::ProtectedVideoType>(protected_video_type_index));
  return true;
}

bool TileDrawQuadFromDict(const base::Value& dict,
                          const DrawQuadCommon& common,
                          TileDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  if (!dict.is_dict())
    return false;
  if (common.resources.count != 1u)
    return false;

  base::Optional<ContentDrawQuadCommon> content_common =
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

bool YUVVideoDrawQuadFromDict(const base::Value& dict,
                              const DrawQuadCommon& common,
                              YUVVideoDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  if (!dict.is_dict())
    return false;
  if (common.resources.count < 3u || common.resources.count > 4u)
    return false;
  const base::Value* ya_tex_coord_rect = dict.FindDictKey("ya_tex_coord_rect");
  const base::Value* uv_tex_coord_rect = dict.FindDictKey("uv_tex_coord_rect");
  const base::Value* ya_tex_size = dict.FindDictKey("ya_tex_size");
  const base::Value* uv_tex_size = dict.FindDictKey("uv_tex_size");
  base::Optional<double> resource_offset =
      dict.FindDoubleKey("resource_offset");
  base::Optional<double> resource_multiplier =
      dict.FindDoubleKey("resource_multiplier");
  base::Optional<int> bits_per_channel = dict.FindIntKey("bits_per_channel");
  const base::Value* video_color_space = dict.FindDictKey("video_color_space");
  const std::string* protected_video_type =
      dict.FindStringKey("protected_video_type");

  if (!ya_tex_coord_rect || !uv_tex_coord_rect || !ya_tex_size ||
      !uv_tex_size || !resource_offset || !resource_multiplier ||
      !bits_per_channel || !video_color_space || !protected_video_type) {
    return false;
  }
  gfx::RectF t_ya_tex_coord_rect, t_uv_tex_coord_rect;
  gfx::ColorSpace t_video_color_space;
  if (!RectFFromDict(*ya_tex_coord_rect, &t_ya_tex_coord_rect) ||
      !RectFFromDict(*uv_tex_coord_rect, &t_uv_tex_coord_rect) ||
      !ColorSpaceFromDict(*video_color_space, &t_video_color_space)) {
    return false;
  }
  int protected_video_type_index =
      StringToProtectedVideoType(*protected_video_type);
  if (protected_video_type_index < 0)
    return false;
  gfx::Size t_ya_tex_size, t_uv_tex_size;
  if (!SizeFromDict(*ya_tex_size, &t_ya_tex_size) ||
      !SizeFromDict(*uv_tex_size, &t_uv_tex_size)) {
    return false;
  }

  const size_t kIndexY = YUVVideoDrawQuad::kYPlaneResourceIdIndex;
  const size_t kIndexU = YUVVideoDrawQuad::kUPlaneResourceIdIndex;
  const size_t kIndexV = YUVVideoDrawQuad::kVPlaneResourceIdIndex;
  const size_t kIndexA = YUVVideoDrawQuad::kAPlaneResourceIdIndex;
  ResourceId y_plane_resource_id = common.resources.ids[kIndexY];
  ResourceId u_plane_resource_id = common.resources.ids[kIndexU];
  ResourceId v_plane_resource_id = common.resources.ids[kIndexV];
  ResourceId a_plane_resource_id = common.resources.ids[kIndexA];
  if (common.resources.count == 3u && a_plane_resource_id)
    return false;

  draw_quad->SetAll(
      common.shared_quad_state, common.rect, common.visible_rect,
      common.needs_blending, t_ya_tex_coord_rect, t_uv_tex_coord_rect,
      t_ya_tex_size, t_uv_tex_size, y_plane_resource_id, u_plane_resource_id,
      v_plane_resource_id, a_plane_resource_id, t_video_color_space,
      static_cast<float>(resource_offset.value()),
      static_cast<float>(resource_multiplier.value()),
      static_cast<uint32_t>(bits_per_channel.value()),
      static_cast<gfx::ProtectedVideoType>(protected_video_type_index),
      gfx::HDRMetadata());
  return true;
}

bool VideoHoleDrawQuadFromDict(const base::Value& dict,
                               const DrawQuadCommon& common,
                               VideoHoleDrawQuad* draw_quad) {
  DCHECK(draw_quad);
  if (!dict.is_dict())
    return false;

  base::Optional<bool> overlay_plane_id_empty =
      dict.FindBoolKey("overlay_plane_id.empty");
  if (!overlay_plane_id_empty)
    return false;

  base::UnguessableToken overlay_plane_id;
  DCHECK(overlay_plane_id.is_empty());
  if (!overlay_plane_id_empty.value()) {
    const std::string* overlay_plane_id_high =
        dict.FindStringKey("overlay_plane_id.high");
    const std::string* overlay_plane_id_low =
        dict.FindStringKey("overlay_plane_id.low");
    uint64_t high = 0, low = 0;
    if (!overlay_plane_id_high ||
        !base::StringToUint64(*overlay_plane_id_high, &high) ||
        !overlay_plane_id_low ||
        !base::StringToUint64(*overlay_plane_id_low, &low)) {
      return false;
    }
    overlay_plane_id = base::UnguessableToken::Deserialize(high, low);
  }
  draw_quad->SetAll(common.shared_quad_state, common.rect, common.visible_rect,
                    common.needs_blending, overlay_plane_id);
  return true;
}

#define UNEXPECTED_DRAW_QUAD_TYPE(NAME)     \
  case DrawQuad::Material::NAME:            \
    NOTREACHED() << "Unexpected " << #NAME; \
    break;
#define GET_QUAD_FROM_DICT(NAME, TYPE)                             \
  case DrawQuad::Material::NAME: {                                 \
    TYPE* quad = quads.AllocateAndConstruct<TYPE>();               \
    if (!TYPE##FromDict(list.GetList()[ii], common.value(), quad)) \
      return false;                                                \
  } break;
bool QuadListFromList(const base::Value& list,
                      QuadList* quad_list,
                      const SharedQuadStateList& shared_quad_state_list) {
  DCHECK(quad_list);
  if (!list.is_list())
    return false;
  size_t size = list.GetList().size();
  if (size == 0) {
    quad_list->clear();
    return true;
  }
  QuadList quads(size);
  for (size_t ii = 0; ii < size; ++ii) {
    if (!list.GetList()[ii].is_dict())
      return false;
    base::Optional<DrawQuadCommon> common =
        GetDrawQuadCommonFromDict(list.GetList()[ii], shared_quad_state_list);
    if (!common)
      return false;
    switch (common->material) {
      GET_QUAD_FROM_DICT(kCompositorRenderPass, CompositorRenderPassDrawQuad)
      GET_QUAD_FROM_DICT(kSolidColor, SolidColorDrawQuad)
      GET_QUAD_FROM_DICT(kStreamVideoContent, StreamVideoDrawQuad)
      GET_QUAD_FROM_DICT(kTextureContent, TextureDrawQuad)
      GET_QUAD_FROM_DICT(kTiledContent, TileDrawQuad)
      GET_QUAD_FROM_DICT(kYuvVideoContent, YUVVideoDrawQuad)
      GET_QUAD_FROM_DICT(kVideoHole, VideoHoleDrawQuad)
      UNEXPECTED_DRAW_QUAD_TYPE(kPictureContent)
      UNEXPECTED_DRAW_QUAD_TYPE(kSurfaceContent)
      default:
        break;
    }
  }
  quad_list->swap(quads);
  return true;
}
#undef GET_QUAD_FROM_DICT
#undef UNEXPECTED_DRAW_QUAD_TYPE

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
      NOTREACHED();
      return "";
  }
}
#undef MAP_BLEND_MODE_TO_STRING

base::Value SharedQuadStateToDict(const SharedQuadState& sqs) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("quad_to_target_transform",
              TransformToList(sqs.quad_to_target_transform));
  dict.SetKey("quad_layer_rect", RectToDict(sqs.quad_layer_rect));
  dict.SetKey("visible_quad_layer_rect",
              RectToDict(sqs.visible_quad_layer_rect));
  dict.SetKey("rounded_corner_bounds",
              RRectFToDict(sqs.mask_filter_info.rounded_corner_bounds()));
  dict.SetKey("clip_rect", RectToDict(sqs.clip_rect));
  dict.SetBoolKey("is_clipped", sqs.is_clipped);
  dict.SetBoolKey("are_contents_opaque", sqs.are_contents_opaque);
  dict.SetDoubleKey("opacity", sqs.opacity);
  dict.SetStringKey("blend_mode", BlendModeToString(sqs.blend_mode));
  dict.SetIntKey("sorting_context_id", sqs.sorting_context_id);
  dict.SetBoolKey("is_fast_rounded_corner", sqs.is_fast_rounded_corner);
  dict.SetDoubleKey("de_jelly_delta_y", sqs.de_jelly_delta_y);
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

bool SharedQuadStateFromDict(const base::Value& dict, SharedQuadState* sqs) {
  DCHECK(sqs);
  if (!dict.is_dict())
    return false;
  const base::Value* quad_to_target_transform =
      dict.FindListKey("quad_to_target_transform");
  const base::Value* quad_layer_rect = dict.FindDictKey("quad_layer_rect");
  const base::Value* visible_quad_layer_rect =
      dict.FindDictKey("visible_quad_layer_rect");
  const base::Value* rounded_corner_bounds =
      dict.FindDictKey("rounded_corner_bounds");
  const base::Value* clip_rect = dict.FindDictKey("clip_rect");
  base::Optional<bool> is_clipped = dict.FindBoolKey("is_clipped");
  base::Optional<bool> are_contents_opaque =
      dict.FindBoolKey("are_contents_opaque");
  base::Optional<double> opacity = dict.FindDoubleKey("opacity");
  const std::string* blend_mode = dict.FindStringKey("blend_mode");
  base::Optional<int> sorting_context_id =
      dict.FindIntKey("sorting_context_id");
  base::Optional<bool> is_fast_rounded_corner =
      dict.FindBoolKey("is_fast_rounded_corner");
  base::Optional<double> de_jelly_delta_y =
      dict.FindDoubleKey("de_jelly_delta_y");

  if (!quad_to_target_transform || !quad_layer_rect ||
      !visible_quad_layer_rect || !rounded_corner_bounds || !clip_rect ||
      !is_clipped || !are_contents_opaque || !opacity || !blend_mode ||
      !sorting_context_id || !is_fast_rounded_corner || !de_jelly_delta_y) {
    return false;
  }
  gfx::Transform t_quad_to_target_transform;
  gfx::Rect t_quad_layer_rect, t_visible_quad_layer_rect, t_clip_rect;
  gfx::RRectF t_rounded_corner_bounds;
  if (!TransformFromList(*quad_to_target_transform,
                         &t_quad_to_target_transform) ||
      !RectFromDict(*quad_layer_rect, &t_quad_layer_rect) ||
      !RectFromDict(*visible_quad_layer_rect, &t_visible_quad_layer_rect) ||
      !RRectFFromDict(*rounded_corner_bounds, &t_rounded_corner_bounds) ||
      !RectFromDict(*clip_rect, &t_clip_rect)) {
    return false;
  }
  int blend_mode_index = StringToBlendMode(*blend_mode);
  DCHECK_GE(static_cast<int>(SkBlendMode::kLastMode), blend_mode_index);
  if (blend_mode_index < 0)
    return false;
  SkBlendMode t_blend_mode = static_cast<SkBlendMode>(blend_mode_index);
  gfx::MaskFilterInfo mask_filter_info(t_rounded_corner_bounds);
  sqs->SetAll(t_quad_to_target_transform, t_quad_layer_rect,
              t_visible_quad_layer_rect, mask_filter_info, t_clip_rect,
              is_clipped.value(), are_contents_opaque.value(),
              static_cast<float>(opacity.value()), t_blend_mode,
              sorting_context_id.value());
  sqs->is_fast_rounded_corner = is_fast_rounded_corner.value();
  sqs->de_jelly_delta_y = static_cast<float>(de_jelly_delta_y.value());
  return true;
}

base::Value SharedQuadStateListToList(
    const SharedQuadStateList& shared_quad_state_list) {
  base::Value list(base::Value::Type::LIST);
  for (size_t ii = 0; ii < shared_quad_state_list.size(); ++ii)
    list.Append(SharedQuadStateToDict(*(shared_quad_state_list.ElementAt(ii))));
  return list;
}

bool SharedQuadStateListFromList(const base::Value& list,
                                 SharedQuadStateList* shared_quad_state_list) {
  DCHECK(shared_quad_state_list);
  if (!list.is_list())
    return false;
  size_t size = list.GetList().size();
  SharedQuadStateList states(alignof(SharedQuadState), sizeof(SharedQuadState),
                             size);
  for (size_t ii = 0; ii < size; ++ii) {
    if (!list.GetList()[ii].is_dict())
      return false;
    SharedQuadState* sqs = states.AllocateAndConstruct<SharedQuadState>();
    if (!SharedQuadStateFromDict(list.GetList()[ii], sqs))
      return false;
  }
  shared_quad_state_list->swap(states);
  return true;
}

base::Value GetRenderPassMetadata(const CompositorRenderPass& render_pass) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(
      "render_pass_id",
      base::NumberToString(static_cast<uint64_t>(render_pass.id)));
  dict.SetIntKey("quad_count", static_cast<int>(render_pass.quad_list.size()));
  dict.SetIntKey("shared_quad_state_count",
                 static_cast<int>(render_pass.shared_quad_state_list.size()));
  return dict;
}

base::Value GetRenderPassListMetadata(
    const CompositorRenderPassList& render_pass_list) {
  base::Value metadata(base::Value::Type::LIST);
  for (size_t ii = 0; ii < render_pass_list.size(); ++ii)
    metadata.Append(GetRenderPassMetadata(*(render_pass_list[ii].get())));
  return metadata;
}

}  // namespace

base::Value CompositorRenderPassToDict(
    const CompositorRenderPass& render_pass) {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (ProcessRenderPassField(kRenderPassID))
    dict.SetStringKey(
        "id", base::NumberToString(static_cast<uint64_t>(render_pass.id)));
  if (ProcessRenderPassField(kRenderPassOutputRect))
    dict.SetKey("output_rect", RectToDict(render_pass.output_rect));
  if (ProcessRenderPassField(kRenderPassDamageRect))
    dict.SetKey("damage_rect", RectToDict(render_pass.damage_rect));
  if (ProcessRenderPassField(kRenderPassTransformToRootTarget)) {
    dict.SetKey("transform_to_root_target",
                TransformToList(render_pass.transform_to_root_target));
  }
  if (ProcessRenderPassField(kRenderPassFilters))
    dict.SetKey("filters", FilterOperationsToList(render_pass.filters));
  if (ProcessRenderPassField(kRenderPassBackdropFilters)) {
    dict.SetKey("backdrop_filters",
                FilterOperationsToList(render_pass.backdrop_filters));
  }
  if (ProcessRenderPassField(kRenderPassBackdropFilterBounds) &&
      render_pass.backdrop_filter_bounds) {
    dict.SetKey("backdrop_filter_bounds",
                RRectFToDict(render_pass.backdrop_filter_bounds.value()));
  }
  if (ProcessRenderPassField(kRenderPassColorSpace)) {
    // CompositorRenderPasses used to have a color space field, but this was
    // removed in favor of color usage. https://crbug.com/1049334
    gfx::ColorSpace render_pass_color_space = gfx::ColorSpace::CreateSRGB();
    dict.SetKey("color_space", ColorSpaceToDict(render_pass_color_space));
  }
  if (ProcessRenderPassField(kRenderPassHasTransparentBackground)) {
    dict.SetBoolKey("has_transparent_background",
                    render_pass.has_transparent_background);
  }
  if (ProcessRenderPassField(kRenderPassCacheRenderPass))
    dict.SetBoolKey("cache_render_pass", render_pass.cache_render_pass);
  if (ProcessRenderPassField(kRenderPassHasDamageFromContributingContent)) {
    dict.SetBoolKey("has_damage_from_contributing_content",
                    render_pass.has_damage_from_contributing_content);
  }
  if (ProcessRenderPassField(kRenderPassGenerateMipmap))
    dict.SetBoolKey("generate_mipmap", render_pass.generate_mipmap);
  if (ProcessRenderPassField(kRenderPassCopyRequests)) {
    // TODO(zmo): Write copy_requests.
  }
  if (ProcessRenderPassField(kRenderPassQuadList)) {
    dict.SetKey("quad_list",
                QuadListToList(render_pass.quad_list,
                               render_pass.shared_quad_state_list));
  }
  if (ProcessRenderPassField(kRenderPassSharedQuadStateList)) {
    dict.SetKey("shared_quad_state_list",
                SharedQuadStateListToList(render_pass.shared_quad_state_list));
  }
  return dict;
}

std::unique_ptr<CompositorRenderPass> CompositorRenderPassFromDict(
    const base::Value& dict) {
  if (!dict.is_dict())
    return nullptr;
  auto pass = CompositorRenderPass::Create();

  if (ProcessRenderPassField(kRenderPassID)) {
    const std::string* id = dict.FindStringKey("id");
    if (!id)
      return nullptr;
    uint64_t pass_id_as_int = 0;
    if (!base::StringToUint64(*id, &pass_id_as_int))
      return nullptr;
    pass->id = CompositorRenderPassId{pass_id_as_int};
  }

  if (ProcessRenderPassField(kRenderPassOutputRect)) {
    const base::Value* output_rect = dict.FindDictKey("output_rect");
    if (!output_rect)
      return nullptr;
    if (!RectFromDict(*output_rect, &(pass->output_rect)))
      return nullptr;
  }

  if (ProcessRenderPassField(kRenderPassDamageRect)) {
    const base::Value* damage_rect = dict.FindDictKey("damage_rect");
    if (!damage_rect)
      return nullptr;
    if (!RectFromDict(*damage_rect, &(pass->damage_rect)))
      return nullptr;
  }

  if (ProcessRenderPassField(kRenderPassTransformToRootTarget)) {
    const base::Value* transform_to_root_target =
        dict.FindListKey("transform_to_root_target");
    if (!transform_to_root_target)
      return nullptr;
    if (!TransformFromList(*transform_to_root_target,
                           &(pass->transform_to_root_target))) {
      return nullptr;
    }
  }

  if (ProcessRenderPassField(kRenderPassFilters)) {
    const base::Value* filters = dict.FindListKey("filters");
    if (!filters)
      return nullptr;
    if (!FilterOperationsFromList(*filters, &(pass->filters)))
      return nullptr;
  }

  if (ProcessRenderPassField(kRenderPassBackdropFilters)) {
    const base::Value* backdrop_filters = dict.FindListKey("backdrop_filters");
    if (!backdrop_filters)
      return nullptr;
    if (!FilterOperationsFromList(*backdrop_filters,
                                  &(pass->backdrop_filters))) {
      return nullptr;
    }
  }

  if (ProcessRenderPassField(kRenderPassBackdropFilterBounds)) {
    const base::Value* backdrop_filter_bounds =
        dict.FindDictKey("backdrop_filter_bounds");
    if (backdrop_filter_bounds) {
      gfx::RRectF bounds;
      if (!RRectFFromDict(*backdrop_filter_bounds, &bounds))
        return nullptr;
      pass->backdrop_filter_bounds = bounds;
    }
  }

  if (ProcessRenderPassField(kRenderPassColorSpace)) {
    const base::Value* color_space = dict.FindDictKey("color_space");
    if (!color_space)
      return nullptr;

    // CompositorRenderPasses used to have a color space field, but this was
    // removed in favor of color usage. https://crbug.com/1049334
    gfx::ColorSpace pass_color_space = gfx::ColorSpace::CreateSRGB();
    if (!ColorSpaceFromDict(*color_space, &pass_color_space))
      return nullptr;
  }

  if (ProcessRenderPassField(kRenderPassHasTransparentBackground)) {
    const base::Optional<bool> has_transparent_background =
        dict.FindBoolKey("has_transparent_background");
    if (!has_transparent_background)
      return nullptr;
    pass->has_transparent_background = has_transparent_background.value();
  }

  if (ProcessRenderPassField(kRenderPassCacheRenderPass)) {
    const base::Optional<bool> cache_render_pass =
        dict.FindBoolKey("cache_render_pass");
    if (!cache_render_pass)
      return nullptr;
    pass->cache_render_pass = cache_render_pass.value();
  }

  if (ProcessRenderPassField(kRenderPassHasDamageFromContributingContent)) {
    const base::Optional<bool> has_damage_from_contributing_content =
        dict.FindBoolKey("has_damage_from_contributing_content");
    if (!has_damage_from_contributing_content)
      return nullptr;
    pass->has_damage_from_contributing_content =
        has_damage_from_contributing_content.value();
  }

  if (ProcessRenderPassField(kRenderPassGenerateMipmap)) {
    const base::Optional<bool> generate_mipmap =
        dict.FindBoolKey("generate_mipmap");
    if (!generate_mipmap)
      return nullptr;
    pass->generate_mipmap = generate_mipmap.value();
  }

  if (ProcessRenderPassField(kRenderPassCopyRequests)) {
    // TODO(zmo): Read copy_requests.
  }

  // shared_quad_state_list has to be processed before quad_list.
  if (ProcessRenderPassField(kRenderPassSharedQuadStateList)) {
    const base::Value* shared_quad_state_list =
        dict.FindListKey("shared_quad_state_list");
    if (!shared_quad_state_list)
      return nullptr;
    if (!SharedQuadStateListFromList(*shared_quad_state_list,
                                     &(pass->shared_quad_state_list))) {
      return nullptr;
    }
  }

  if (ProcessRenderPassField(kRenderPassQuadList)) {
    const base::Value* quad_list = dict.FindListKey("quad_list");
    if (!quad_list)
      return nullptr;
    if (!QuadListFromList(*quad_list, &(pass->quad_list),
                          pass->shared_quad_state_list)) {
      return nullptr;
    }
  }

  return pass;
}

base::Value CompositorRenderPassListToDict(
    const CompositorRenderPassList& render_pass_list) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("render_pass_count",
                 static_cast<int>(render_pass_list.size()));
  dict.SetKey("metadata", GetRenderPassListMetadata(render_pass_list));

  base::Value list(base::Value::Type::LIST);
  for (size_t ii = 0; ii < render_pass_list.size(); ++ii)
    list.Append(CompositorRenderPassToDict(*(render_pass_list[ii])));
  dict.SetKey("render_pass_list", std::move(list));
  return dict;
}

bool CompositorRenderPassListFromDict(
    const base::Value& dict,
    CompositorRenderPassList* render_pass_list) {
  DCHECK(render_pass_list);
  DCHECK(render_pass_list->empty());
  if (!dict.is_dict())
    return false;
  const base::Value* list = dict.FindListKey("render_pass_list");
  if (!list || !list->is_list())
    return false;
  for (size_t ii = 0; ii < list->GetList().size(); ++ii) {
    render_pass_list->push_back(
        CompositorRenderPassFromDict(list->GetList()[ii]));
    if (!(*render_pass_list)[ii].get()) {
      render_pass_list->clear();
      return false;
    }
  }
  return true;
}

}  // namespace viz
