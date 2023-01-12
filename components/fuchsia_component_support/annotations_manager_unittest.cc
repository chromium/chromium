// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/annotations_manager.h"

#include "base/fuchsia/mem_buffer_util.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fuchsia_component_support {

namespace {

class AnnotationsManagerTest : public testing::Test {
 protected:
  AnnotationsManagerTest() = default;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
};

constexpr char kGlobalNamespace[] = "global";

constexpr char kAnnotationName1[] = "annotation1";
constexpr char kAnnotationName2[] = "annotation2";

constexpr char kAnnotationValue1[] = "annotation_value1";
constexpr char kAnnotationValue2[] = "annotation_value2";

TEST(MakeAnnotationTest, KeyNamespaceAndName) {
  auto annotation = MakeAnnotation(kAnnotationName1, "ignored");
  EXPECT_EQ(annotation.key.namespace_, kGlobalNamespace);
  EXPECT_EQ(annotation.key.value, kAnnotationName1);
}

TEST(MakeAnnotationTest, BooleanValue) {
  auto annotation = MakeBoolAnnotation(kAnnotationName1, true);
  ASSERT_TRUE(annotation.value.is_text());
  EXPECT_EQ(annotation.value.text(), "true");
}

TEST(MakeAnnotationTest, IntegerValue) {
  auto annotation = MakeIntAnnotation(kAnnotationName1, 12345678);
  ASSERT_TRUE(annotation.value.is_text());
  EXPECT_EQ(annotation.value.text(), "12345678");
}

TEST(MakeAnnotationTest, TextValue) {
  constexpr char kSmallText[] = "This is a short text annotation";
  auto annotation = MakeAnnotation(kAnnotationName1, kSmallText);
  ASSERT_TRUE(annotation.value.is_text());
  EXPECT_EQ(annotation.value.text(), kSmallText);
}

TEST(MakeAnnotationTest, LargeTextValue) {
  const std::string large_text(1024, 'a');
  auto annotation = MakeAnnotation(kAnnotationName1, large_text);
  ASSERT_TRUE(annotation.value.is_buffer());
  auto value = base::StringFromMemBuffer(annotation.value.buffer());
  EXPECT_EQ(value, large_text);
}

// Basic verification that a single client can connect.
TEST_F(AnnotationsManagerTest, SingleClient) {
  AnnotationsManager annotations;

  fuchsia::element::AnnotationControllerPtr ptr;
  annotations.Connect(ptr.NewRequest());
}

// Verifies that multiple clients can connect.
TEST_F(AnnotationsManagerTest, MultipleClients) {
  AnnotationsManager annotations;

  fuchsia::element::AnnotationControllerPtr ptr1;
  annotations.Connect(ptr1.NewRequest());
  fuchsia::element::AnnotationControllerPtr ptr2;
  annotations.Connect(ptr2.NewRequest());
}

// Verifies that WatchAnnotations() hanging-get behaviour:
// - first call returns all annotations.
// - watch when nothing has changed "hangs" until the next change.
// - watch after a change returns immediately.
TEST_F(AnnotationsManagerTest, WatchAnnotationsHangingGet) {
  AnnotationsManager annotations;

  fuchsia::element::AnnotationControllerPtr controller;
  annotations.Connect(controller.NewRequest());

  // Dispatch an initial watch, that will return immediately.
  bool received_annotations = false;
  controller->WatchAnnotations([&received_annotations](auto annotations) {
    EXPECT_EQ(annotations.response().annotations.size(), 0u);
    received_annotations = true;
  });
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(received_annotations);

  // Set a new watch, which should not return immediately, since nothing has
  // changed.
  received_annotations = false;
  controller->WatchAnnotations([&received_annotations](auto annotations) {
    EXPECT_EQ(annotations.response().annotations.size(), 1u);
    received_annotations = true;
  });
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(received_annotations);

  // Set an annotation and spin the loop, to verify that the update is reported.
  std::vector<fuchsia::element::Annotation> to_set;
  to_set.push_back(MakeAnnotation(kAnnotationName1, kAnnotationValue1));
  annotations.UpdateAnnotations(std::move(to_set));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(received_annotations);

  // Change the annotation and spin the loop, then call WatchAnnotations() to
  // verify that it returns immediately.
  to_set.clear();
  to_set.push_back(MakeAnnotation(kAnnotationName1, kAnnotationValue2));
  annotations.UpdateAnnotations(std::move(to_set));
  received_annotations = false;
  controller->WatchAnnotations([&received_annotations](auto annotations) {
    EXPECT_EQ(annotations.response().annotations.size(), 1u);
    received_annotations = true;
  });
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(received_annotations);
}

// Verifies that WatchAnnotations() returns all annotations when any annotation
// is updated, not just the changed one.
TEST_F(AnnotationsManagerTest, WatchAnnotationsReturnsAllAnnotations) {
  AnnotationsManager annotations;

  fuchsia::element::AnnotationControllerPtr controller;
  annotations.Connect(controller.NewRequest());

  // Set multiple annotations.
  std::vector<fuchsia::element::Annotation> annotations_to_set;
  annotations_to_set.push_back(
      MakeAnnotation(kAnnotationName1, kAnnotationValue1));
  annotations_to_set.push_back(
      MakeAnnotation(kAnnotationName2, kAnnotationValue2));
  annotations.UpdateAnnotations(std::move(annotations_to_set));

  // Dispatch an initial watch, that will return immediately.
  bool received_annotations = false;
  controller->WatchAnnotations([&received_annotations](auto annotations) {
    EXPECT_EQ(annotations.response().annotations.size(), 2u);
    received_annotations = true;
  });
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(received_annotations);

  // Update a single annotation, then call WatchAnnotations() again to verify
  // that all annotations including unchanged ones are returned.
  std::vector<fuchsia::element::Annotation> to_set;
  to_set.push_back(MakeAnnotation(kAnnotationName1, kAnnotationValue2));
  annotations.UpdateAnnotations(std::move(to_set));
  received_annotations = false;
  controller->WatchAnnotations([&received_annotations](auto annotations) {
    EXPECT_EQ(annotations.response().annotations.size(), 2u);
    received_annotations = true;
  });
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(received_annotations);
}

// Verifies insertion of a new annotation via UpdateAnnotations().
TEST_F(AnnotationsManagerTest, UpdateAnnotationsSetsAnnotations) {
  AnnotationsManager annotations;

  auto all_annotations = annotations.GetAnnotations();
  ASSERT_EQ(all_annotations.size(), 0u);

  std::vector<fuchsia::element::Annotation> to_set;
  to_set.push_back(MakeAnnotation(kAnnotationName1, kAnnotationValue1));
  annotations.UpdateAnnotations(std::move(to_set));

  all_annotations = annotations.GetAnnotations();
  ASSERT_EQ(all_annotations.size(), 1u);
  EXPECT_EQ(all_annotations[0].key.value, kAnnotationName1);
  ASSERT_TRUE(all_annotations[0].value.is_text());
  EXPECT_EQ(all_annotations[0].value.text(), kAnnotationValue1);
}

// Verifies update of existing annotation via UpdateAnnotations().
TEST_F(AnnotationsManagerTest, UpdateAnnotationsUpdatesAnnotations) {
  AnnotationsManager annotations;

  std::vector<fuchsia::element::Annotation> to_set;
  to_set.push_back(MakeAnnotation(kAnnotationName1, kAnnotationValue1));
  annotations.UpdateAnnotations(std::move(to_set));
  to_set.push_back(MakeAnnotation(kAnnotationName1, kAnnotationValue2));
  annotations.UpdateAnnotations(std::move(to_set));

  auto all_annotations = annotations.GetAnnotations();
  ASSERT_EQ(all_annotations.size(), 1u);
  EXPECT_EQ(all_annotations[0].key.value, kAnnotationName1);
  ASSERT_TRUE(all_annotations[0].value.is_text());
  EXPECT_EQ(all_annotations[0].value.text(), kAnnotationValue2);
}

// Verifies that multiple annotations can be updated or insert in a single call.
TEST_F(AnnotationsManagerTest, UpdateAnnotationsMultipleAnnotations) {
  AnnotationsManager annotations;

  std::vector<fuchsia::element::Annotation> annotations_to_set;
  annotations_to_set.push_back(
      MakeAnnotation(kAnnotationName1, kAnnotationValue1));
  annotations_to_set.push_back(
      MakeAnnotation(kAnnotationName2, kAnnotationValue2));
  annotations.UpdateAnnotations(std::move(annotations_to_set));

  auto all_annotations = annotations.GetAnnotations();
  ASSERT_EQ(all_annotations.size(), 2u);
  EXPECT_EQ(all_annotations[0].key.value, kAnnotationName1);
  ASSERT_TRUE(all_annotations[0].value.is_text());
  EXPECT_EQ(all_annotations[0].value.text(), kAnnotationValue1);
  EXPECT_EQ(all_annotations[1].key.value, kAnnotationName2);
  ASSERT_TRUE(all_annotations[1].value.is_text());
  EXPECT_EQ(all_annotations[1].value.text(), kAnnotationValue2);
}

// Verifies that the controller fails the operation if duplicate annotations
// appear in the same bulk-UpdateAnnotations operation.
TEST_F(AnnotationsManagerTest,
       UpdateAnnotationsMultipleAnnotations_WithDuplicate) {
  AnnotationsManager annotations;

  std::vector<fuchsia::element::Annotation> annotations_to_set;
  annotations_to_set.push_back(
      MakeAnnotation(kAnnotationName1, kAnnotationValue1));
  annotations_to_set.push_back(
      MakeAnnotation(kAnnotationName1, kAnnotationValue2));

  EXPECT_FALSE(annotations.UpdateAnnotations(std::move(annotations_to_set)));
}

}  // namespace

}  // namespace fuchsia_component_support
