// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/proto/proto_convertor.h"

#include <algorithm>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "components/services/screen_ai/proto/test_proto_loader.h"
#include "components/services/screen_ai/proto/view_hierarchy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/test_ax_tree_update_json_reader.h"
#include "ui/gfx/geometry/point_f.h"

namespace {

// Set to 'true' to get debug protos.
#define WRITE_DEBUG_PROTO false

// Test definitions for ProtoConvertorViewHierarchyTest.
constexpr int kProtoConversionTestCasesCount = 5;
const char* kProtoConversionSampleInputFileNameFormat = "sample%i_ax_tree.json";
const char* kProtoConversionSampleExpectedFileNameFormat =
    "sample%i_expected_proto.pbtxt";

// A dummy tree node definition for PreOrderTreeGeneration.
constexpr int kMaxChildInTemplate = 3;
struct NodeTemplate {
  ui::AXNodeID node_id;
  int child_count;
  ui::AXNodeID child_ids[kMaxChildInTemplate];
};

ui::AXTreeUpdate CreateAXTreeUpdateFromTemplate(int root_id,
                                                NodeTemplate* nodes_template,
                                                int nodes_count) {
  ui::AXTreeUpdate update;
  update.root_id = root_id;

  for (int i = 0; i < nodes_count; i++) {
    ui::AXNodeData node;
    node.id = nodes_template[i].node_id;
    for (int j = 0; j < nodes_template[i].child_count; j++)
      node.child_ids.push_back(nodes_template[i].child_ids[j]);
    update.nodes.push_back(node);
  }
  return update;
}

int GetAxNodeID(const ::screenai::UiElement& ui_element) {
  for (const auto& attribute : ui_element.attributes()) {
    if (attribute.name() == "axnode_id")
      return attribute.int_value();
  }
  return static_cast<int>(ui::kInvalidAXNodeID);
}

base::FilePath GetTestFilePath(const base::StringPiece file_name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path));
  return path.AppendASCII("components/test/data/screen_ai")
      .AppendASCII(file_name);
}

void WriteDebugProto(const std::string& serialized_proto,
                     const std::string& file_name) {
  if (!WRITE_DEBUG_PROTO)
    return;

  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &path));
  path = path.AppendASCII(file_name);

  size_t bytes_written =
      base::WriteFile(path, serialized_proto.c_str(), serialized_proto.size());

  if (bytes_written == serialized_proto.size())
    LOG(INFO) << "Debug proto is written to: " << path;
  else
    LOG(ERROR) << "Could not write debug proto to: " << path;
}

template <class T>
void ExpectBoundingBoxes(const T& box1,
                         const T& box2,
                         const gfx::PointF& max_diff) {
  EXPECT_NEAR(box1.top(), box2.top(), max_diff.y());
  EXPECT_NEAR(box1.left(), box2.left(), max_diff.x());
  EXPECT_NEAR(box1.bottom(), box2.bottom(), max_diff.y());
  EXPECT_NEAR(box1.right(), box2.right(), max_diff.x());
}

// TODO(https://crbug.com/1278249): Consider making the comparison not sensitive
// to order.
template <class T>
void ExpectLists(const T& list1, const T& list2) {
  EXPECT_EQ(list1.value_size(), list2.value_size());
  int min_length = std::min(list1.value_size(), list2.value_size());
  for (int i = 0; i < min_length; i++)
    EXPECT_EQ(list1.value(i), list2.value(i));
}

void ExpectAttributes(const ::screenai::UiElementAttribute& attrib1,
                      const ::screenai::UiElementAttribute& attrib2) {
  SCOPED_TRACE(
      base::StringPrintf("Comparing attribute: %s", attrib1.name().c_str()));
  EXPECT_EQ(attrib1.value_case(), attrib2.value_case());

  switch (attrib1.value_case()) {
    case screenai::UiElementAttribute::ValueCase::kBoolValue:
      EXPECT_EQ(attrib1.bool_value(), attrib2.bool_value());
      break;
    case screenai::UiElementAttribute::kIntValue:
      EXPECT_EQ(attrib1.int_value(), attrib2.int_value());
      break;
    case screenai::UiElementAttribute::kStringValue:
      EXPECT_EQ(attrib1.string_value(), attrib2.string_value());
      break;
    case screenai::UiElementAttribute::kFloatValue:
      EXPECT_EQ(attrib1.float_value(), attrib2.float_value());
      break;
    case screenai::UiElementAttribute::kIntListValue: {
      ExpectLists(attrib1.int_list_value(), attrib2.int_list_value());
      break;
    }
    case screenai::UiElementAttribute::kStringListValue: {
      ExpectLists(attrib1.int_list_value(), attrib2.int_list_value());
      break;
    }
    case screenai::UiElementAttribute::kFloatListValue: {
      NOTREACHED() << "Chrome has no float list.";
      break;
    }
    case screenai::UiElementAttribute::VALUE_NOT_SET:
      break;
  }
}

void ExpectViewHierarchyProtos(screenai::ViewHierarchy& generated,
                               screenai::ViewHierarchy& expected) {
  EXPECT_EQ(generated.ui_elements_size(), expected.ui_elements_size());

  // Bounding boxes can have a one pixel difference threshold as there might be
  // different approaches in rounding floats to integers.
  // To compare |bounding_box_pixels| values which represent bounding boxes in
  // pixels, we use |kPixelDifferenceThreshold|.
  const gfx::PointF kPixelDifferenceThreshold(1, 1);

  // |bounding_box| values represent the relative position of a bounding box to
  // the tree, and as each of them (the element and the tree) can have a one
  // pixel error, the total error can be up to two pixels. To get the tree size
  // to compute |kRelativeDifferenceThreshold|, we use the 0th element which
  // should be the root and cover the entire tree.
  const auto& root = expected.ui_elements(0);
  const gfx::PointF kRelativeDifferenceThreshold(
      2.0 / root.bounding_box_pixels().right(),
      2.0 / root.bounding_box_pixels().bottom());
  EXPECT_EQ(root.bounding_box_pixels().top(), 0);
  EXPECT_EQ(root.bounding_box_pixels().left(), 0);

  for (int i = 0; i < generated.ui_elements_size(); i++) {
    SCOPED_TRACE(base::StringPrintf("Comparing ui_elements at index: %i", i));
    const screenai::UiElement& generated_uie = generated.ui_elements(i);
    const screenai::UiElement& expected_uie = expected.ui_elements(i);

    EXPECT_EQ(generated_uie.id(), expected_uie.id());
    EXPECT_EQ(generated_uie.type(), expected_uie.type());
    EXPECT_EQ(generated_uie.parent_id(), expected_uie.parent_id());

    EXPECT_EQ(generated_uie.child_ids_size(), expected_uie.child_ids_size());
    int min_length =
        std::min(generated_uie.child_ids_size(), expected_uie.child_ids_size());
    for (int child_index = 0; child_index < min_length; child_index++)
      EXPECT_EQ(generated_uie.child_ids(child_index),
                expected_uie.child_ids(child_index));

    ExpectBoundingBoxes(generated_uie.bounding_box(),
                        expected_uie.bounding_box(),
                        kRelativeDifferenceThreshold);
    ExpectBoundingBoxes(generated_uie.bounding_box_pixels(),
                        expected_uie.bounding_box_pixels(),
                        kPixelDifferenceThreshold);

    // Attributes may have different orders in the two protos.
    std::map<std::string, int> attribute_indices_map;
    for (int j = 0; j < expected_uie.attributes_size(); j++)
      attribute_indices_map[expected_uie.attributes(j).name()] = j;

    int expected_attributes_count = expected_uie.attributes_size();
    for (int j = 0; j < generated_uie.attributes_size(); j++) {
      const ::screenai::UiElementAttribute& generated_attrib =
          generated_uie.attributes(j);

      const auto& expected_attrib_index =
          attribute_indices_map.find(generated_attrib.name());
      bool attribute_found_in_expected =
          (expected_attrib_index != attribute_indices_map.end());

      if (attribute_found_in_expected) {
        ExpectAttributes(generated_attrib, expected_uie.attributes(
                                               expected_attrib_index->second));
        // Remove expected attributes from |attribute_indices_map|, leaving
        // missing attributes for reporting.
        attribute_indices_map.erase(expected_attrib_index);
        continue;
      }
      // TODO(https://crbug.com/1278249): Follow up why visibility is
      // sometimes not passed.
      if (generated_attrib.name() != "/extras/styles/visibility")
        EXPECT_TRUE(attribute_found_in_expected) << generated_attrib.name();
      else
        expected_attributes_count++;
    }

    EXPECT_EQ(expected_attributes_count, generated_uie.attributes_size());
  }
}

void LoadViewHierarchyTextProto(const base::FilePath& file_path,
                                screenai::ViewHierarchy& proto) {
  std::string file_content;
  ASSERT_TRUE(base::ReadFileToString(file_path, &file_content))
      << "Failed to read expected proto from " << file_path;

  base::FilePath descriptor_path;
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &descriptor_path));
  descriptor_path = descriptor_path.AppendASCII(
      "gen/components/services/screen_ai/proto/view_hierarchy.descriptor");

  test_proto_loader::TestProtoLoader loader;
  ASSERT_TRUE(loader.ParseFromText(descriptor_path, file_content, proto))
      << "Failed to parse expected proto.";
}

}  // namespace

namespace screen_ai {

using ProtoConvertorTest = testing::Test;

TEST_F(ProtoConvertorTest, ScreenAIVisualAnnotationToAXTreeUpdate) {
  chrome_screen_ai::VisualAnnotation annotation;
  gfx::Rect snapshot_bounds(800, 900);

  {
    chrome_screen_ai::UIComponent* component_0 = annotation.add_ui_component();
    chrome_screen_ai::UIComponent::PredictedType* type_0 =
        component_0->mutable_predicted_type();
    type_0->set_enum_type(chrome_screen_ai::UIComponent::BUTTON);
    chrome_screen_ai::Rect* box_0 = component_0->mutable_bounding_box();
    box_0->set_x(0);
    box_0->set_y(1);
    box_0->set_width(2);
    box_0->set_height(3);
    box_0->set_angle(90.0f);

    chrome_screen_ai::UIComponent* component_1 = annotation.add_ui_component();
    chrome_screen_ai::UIComponent::PredictedType* type_1 =
        component_1->mutable_predicted_type();
    type_1->set_string_type("Signature");
    chrome_screen_ai::Rect* box_1 = component_1->mutable_bounding_box();
    // `x`, `y`, and `angle` should be defaulted to 0 since they are singular
    // proto3 fields, not proto2.
    box_1->set_width(5);
    box_1->set_height(5);
  }

  {
    std::string serialized_annotation;
    ASSERT_TRUE(annotation.SerializeToString(&serialized_annotation));
    const ui::AXTreeUpdate update = ScreenAIVisualAnnotationToAXTreeUpdate(
        serialized_annotation, snapshot_bounds);

    const std::string expected_update(
        "id=1 dialog (0, 0)-(800, 900) child_ids=2,3\n"
        "  id=2 button offset_container_id=1 (0, 1)-(2, 3) transform=[ +0.0000 "
        "-1.0000 +0.0000 +0.0000  \n"
        "  +1.0000 +0.0000 +0.0000 +0.0000  \n"
        "  +0.0000 +0.0000 +1.0000 +0.0000  \n"
        "  +0.0000 +0.0000 +0.0000 +1.0000 ]\n"
        "\n"
        "  id=3 genericContainer offset_container_id=1 (0, 0)-(5, 5) "
        "role_description=Signature\n");
    EXPECT_EQ(expected_update, update.ToString());
  }

  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();
    line_0->set_direction(chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT);

    chrome_screen_ai::WordBox* word_0_0 = line_0->add_words();
    chrome_screen_ai::Rect* box_0_0 = word_0_0->mutable_bounding_box();
    box_0_0->set_x(100);
    box_0_0->set_y(100);
    box_0_0->set_width(250);
    box_0_0->set_height(20);
    word_0_0->set_utf8_string("Hello");
    word_0_0->set_language("en");
    word_0_0->set_has_space_after(true);
    word_0_0->set_estimate_color_success(true);
    word_0_0->set_background_rgb_value(50000);
    word_0_0->set_foreground_rgb_value(25000);
    word_0_0->set_direction(chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT);

    chrome_screen_ai::WordBox* word_0_1 = line_0->add_words();
    chrome_screen_ai::Rect* box_0_1 = word_0_1->mutable_bounding_box();
    box_0_1->set_x(350);
    box_0_1->set_y(100);
    box_0_1->set_width(250);
    box_0_1->set_height(20);
    word_0_1->set_utf8_string("world");
    word_0_1->set_language("en");
    // `word_0_1.has_space_after()` should be defaulted to false.
    word_0_1->set_estimate_color_success(true);
    word_0_1->set_background_rgb_value(50000);
    word_0_1->set_foreground_rgb_value(25000);
    word_0_1->set_direction(chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT);

    chrome_screen_ai::Rect* box_0 = line_0->mutable_bounding_box();
    box_0->set_x(100);
    box_0->set_y(100);
    box_0->set_width(500);
    box_0->set_height(20);
    line_0->set_utf8_string("Hello world");
    line_0->set_language("en");
    line_0->set_block_id(1);
    line_0->set_order_within_block(1);
  }

  {
    std::string serialized_annotation;
    ASSERT_TRUE(annotation.SerializeToString(&serialized_annotation));
    const ui::AXTreeUpdate update = ScreenAIVisualAnnotationToAXTreeUpdate(
        serialized_annotation, snapshot_bounds);

    const std::string expected_update(
        "id=4 dialog (0, 0)-(800, 900) child_ids=5,6\n"
        "  id=5 button offset_container_id=4 (0, 1)-(2, 3) transform=[ "
        "+0.0000 "
        "-1.0000 +0.0000 +0.0000  \n"
        "  +1.0000 +0.0000 +0.0000 +0.0000  \n"
        "  +0.0000 +0.0000 +1.0000 +0.0000  \n"
        "  +0.0000 +0.0000 +0.0000 +1.0000 ]\n"
        "\n"
        "  id=6 genericContainer offset_container_id=4 (0, 0)-(5, 5) "
        "role_description=Signature\n"
        "id=7 region (0, 0)-(800, 900) is_page_breaking_object=true "
        "child_ids=8\n"
        "  id=8 staticText offset_container_id=7 (100, 100)-(500, 20) "
        "name_from=contents text_direction=rtl name=Hello world language=en "
        "child_ids=9\n"
        "    id=9 inlineTextBox (100, 100)-(500, 20) name_from=contents "
        "background_color=&C350 color=&61A8 text_direction=rtl language=en "
        "name=Hello world word_starts=0,6 word_ends=6,11\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
}

// Tests if the given tree is properly traversed and new ids are assigned.
TEST_F(ProtoConvertorTest, PreOrderTreeGeneration) {
  // Input Tree:
  // +-- 1
  //     +-- 2
  //         +-- 7
  //         +-- 8
  //             +-- 3
  //     +-- 4
  //         +-- 5
  //         +-- 6
  //         +-- 9
  //     +-- -20

  // Input tree is added in shuffled order to avoid order assumption.
  NodeTemplate input_tree[] = {
      {4, 3, {5, 6, 9}}, {1, 3, {2, 4, -20}}, {7, 0, {}}, {8, 1, {3}},
      {6, 0, {}},        {5, 0, {}},          {3, 0, {}}, {2, 2, {7, 8}},
      {9, 0, {}},        {-20, 0, {}}};
  const int nodes_count = sizeof(input_tree) / sizeof(NodeTemplate);

  // Expected order of nodes in the output.
  int expected_order[] = {1, 2, 7, 8, 3, 4, 5, 6, 9, -20};

  // Create the tree, convert it, and decode from proto.
  ui::AXTreeUpdate tree_update =
      CreateAXTreeUpdateFromTemplate(1, input_tree, nodes_count);
  std::string serialized_proto = Screen2xSnapshotToViewHierarchy(tree_update);
  screenai::ViewHierarchy view_hierarchy;
  ASSERT_TRUE(view_hierarchy.ParseFromString(serialized_proto));

  // Verify.
  EXPECT_EQ(view_hierarchy.ui_elements().size(), nodes_count);
  for (int i = 0; i < nodes_count; i++) {
    const screenai::UiElement& ui_element = view_hierarchy.ui_elements(i);

    // Expect node to be correctly re-ordered.
    EXPECT_EQ(expected_order[i], GetAxNodeID(ui_element));

    // Expect node at index 'i' has id 'i'
    EXPECT_EQ(ui_element.id(), i);
  }
}

class ProtoConvertorViewHierarchyTest : public ::testing::TestWithParam<int> {
 public:
  ProtoConvertorViewHierarchyTest() = default;
  ~ProtoConvertorViewHierarchyTest() = default;

 protected:
  const base::FilePath GetInputFilePath() {
    return GetTestFilePath(base::StringPrintf(
        kProtoConversionSampleInputFileNameFormat, GetParam()));
  }

  const base::FilePath GetExpectedFilePath() {
    return GetTestFilePath(base::StringPrintf(
        kProtoConversionSampleExpectedFileNameFormat, GetParam()));
  }
};

INSTANTIATE_TEST_SUITE_P(TestCases,
                         ProtoConvertorViewHierarchyTest,
                         testing::Range(0, kProtoConversionTestCasesCount));

TEST_P(ProtoConvertorViewHierarchyTest, EndToEndTest) {
  const base::FilePath kInputJsonPath = GetInputFilePath();
  const base::FilePath kExpectedProtoPath = GetExpectedFilePath();

  // Load JSON file.
  std::string file_content;
  ASSERT_TRUE(base::ReadFileToString(kInputJsonPath, &file_content))
      << "Failed to load input AX tree: " << kInputJsonPath;
  absl::optional<base::Value> json = base::JSONReader::Read(file_content);
  ASSERT_TRUE(json.has_value());

  // Convert JSON file to AX tree update.
  ui::AXTreeUpdate tree_update = ui::AXTreeUpdateFromJSON(
      json.value(),
      &screen_ai::GetScreen2xToChromeRoleConversionMapForTesting());
  ASSERT_GT(tree_update.nodes.size(), 0u);

  // Convert AX Tree to Screen2x proto.
  std::string serialized_proto =
      screen_ai::Screen2xSnapshotToViewHierarchy(tree_update);
  screenai::ViewHierarchy generated_view_hierarchy;
  ASSERT_TRUE(generated_view_hierarchy.ParseFromString(serialized_proto))
      << "Failed to parse created proto.";

  WriteDebugProto(
      serialized_proto,
      base::StringPrintf("proto_convertor_sample_%i_output.pbtxt", GetParam()));

  // Load expected Proto.
  screenai::ViewHierarchy expected_view_hierarchy;
  ASSERT_NO_FATAL_FAILURE(
      LoadViewHierarchyTextProto(kExpectedProtoPath, expected_view_hierarchy));

  // Compare protos.
  ASSERT_NO_FATAL_FAILURE(ExpectViewHierarchyProtos(generated_view_hierarchy,
                                                    expected_view_hierarchy));
}

}  // namespace screen_ai
