// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_annotation/core/page_annotator.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_image_annotation {

namespace {

namespace ia_mojom = image_annotation::mojom;

using testing::ElementsAre;
using testing::Eq;
using testing::NiceMock;
using testing::SizeIs;

// A gMock matcher that compares an ImageMetadata to the given node and source
// IDs.
MATCHER_P2(IsImageMetadata, expected_node_id, expected_source_id, "") {
  return arg.node_id == expected_node_id && arg.source_id == expected_source_id;
}

// A gMock matcher that (deep) compares an AnnotateImageResultPtr to the
// expected result (which is of type AnnotateImageResult*).
MATCHER_P(IsAnnotateImageResult, expected, "") {
  return arg->Equals(*expected);
}

class MockObserver : public PageAnnotator::Observer {
 public:
  MOCK_METHOD1(OnImageAdded,
               void(const PageAnnotator::ImageMetadata& metadata));
  MOCK_METHOD1(OnImageModified,
               void(const PageAnnotator::ImageMetadata& metadata));
  MOCK_METHOD1(OnImageRemoved, void(uint64_t node_id));
  MOCK_METHOD2(OnImageAnnotated,
               void(uint64_t node_id, ia_mojom::AnnotateImageResultPtr result));
};

// An annotator that just stores and exposes the arguments with which its
// AnnotateImage method was called.
class TestAnnotator : public ia_mojom::Annotator {
 public:
  mojo::PendingRemote<ia_mojom::Annotator> GetRemote() {
    mojo::PendingRemote<ia_mojom::Annotator> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void AnnotateImage(
      const std::string& source_id,
      const std::string& description_language_tag,
      mojo::PendingRemote<ia_mojom::ImageProcessor> image_processor,
      AnnotateImageCallback callback) override {
    CHECK_EQ(description_language_tag, std::string());

    source_ids_.push_back(source_id);

    image_processors_.push_back(
        mojo::Remote<ia_mojom::ImageProcessor>(std::move(image_processor)));
    image_processors_.back().set_disconnect_handler(
        base::BindOnce(&TestAnnotator::ResetImageProcessor,
                       base::Unretained(this), image_processors_.size() - 1));

    callbacks_.push_back(std::move(callback));
  }

  // Tests should not delete entries in these lists.
  std::vector<std::string> source_ids_;
  std::vector<mojo::Remote<ia_mojom::ImageProcessor>> image_processors_;
  std::vector<AnnotateImageCallback> callbacks_;

 private:
  void ResetImageProcessor(const size_t index) {
    image_processors_[index].reset();
  }

  mojo::ReceiverSet<ia_mojom::Annotator> receivers_;
};

// Tests that correct image tracking messages are sent to observers.
TEST(PageAnnotatorTest, ImageTracking) {
  const auto get_pixels = base::BindRepeating([]() { return SkBitmap(); });

  base::test::TaskEnvironment test_task_env;

  PageAnnotator page_annotator((mojo::NullRemote()));

  MockObserver o1;
  page_annotator.AddObserver(&o1);

  EXPECT_CALL(o1, OnImageAdded(IsImageMetadata(1ul, "test.jpg")));
  page_annotator.ImageAddedOrPossiblyModified({1ul, "test.jpg"}, get_pixels);

  EXPECT_CALL(o1, OnImageAdded(IsImageMetadata(2ul, "example.png")));
  page_annotator.ImageAddedOrPossiblyModified({2ul, "example.png"}, get_pixels);

  EXPECT_CALL(o1, OnImageModified(IsImageMetadata(1ul, "demo.gif")));
  page_annotator.ImageAddedOrPossiblyModified({1ul, "demo.gif"}, get_pixels);

  EXPECT_CALL(o1, OnImageRemoved(2ul));
  page_annotator.ImageRemoved(2ul);

  MockObserver o2;
  EXPECT_CALL(o2, OnImageAdded(IsImageMetadata(1ul, "demo.gif")));
  page_annotator.AddObserver(&o2);
}

// Tests service and observer communication when performing image annotation.
TEST(PageAnnotatorTest, Annotation) {
  // Returning a null SkBitmap is ok as long as we don't request JPG data from
  // the local ImageProcessor.
  const auto get_pixels = base::BindRepeating([]() { return SkBitmap(); });

  base::test::TaskEnvironment test_task_env;

  TestAnnotator test_annotator;
  PageAnnotator page_annotator(test_annotator.GetRemote());
  test_task_env.RunUntilIdle();

  // We use NiceMocks here since we don't place expectations on image added /
  // removed calls, which will otherwise cause many (benign) warnings to be
  // logged.
  NiceMock<MockObserver> o1, o2;
  page_annotator.AddObserver(&o1);
  page_annotator.AddObserver(&o2);

  // First image added.
  page_annotator.ImageAddedOrPossiblyModified({1ul, "test.jpg"}, get_pixels);

  // Observer 1 requests annotation of the first image.
  page_annotator.AnnotateImage(&o1, 1ul);
  test_task_env.RunUntilIdle();

  // The annotator should have been provided observer 1's request info.
  EXPECT_THAT(test_annotator.source_ids_, ElementsAre("test.jpg"));
  ASSERT_THAT(test_annotator.image_processors_, SizeIs(1));
  EXPECT_THAT(test_annotator.image_processors_[0].is_bound(), Eq(true));
  EXPECT_THAT(test_annotator.callbacks_, SizeIs(1));

  // Observer 2 requests annotation of the same image.
  page_annotator.AnnotateImage(&o2, 1ul);
  test_task_env.RunUntilIdle();

  // The annotator should have been provided observer 2's request info.
  EXPECT_THAT(test_annotator.source_ids_, ElementsAre("test.jpg", "test.jpg"));
  ASSERT_THAT(test_annotator.image_processors_, SizeIs(2));
  EXPECT_THAT(test_annotator.image_processors_[0].is_bound(), Eq(true));
  EXPECT_THAT(test_annotator.image_processors_[1].is_bound(), Eq(true));
  EXPECT_THAT(test_annotator.callbacks_, SizeIs(2));

  // Second image added.
  page_annotator.ImageAddedOrPossiblyModified({2ul, "example.png"}, get_pixels);

  // Observer 2 requests annotation of the second image.
  page_annotator.AnnotateImage(&o2, 2ul);
  test_task_env.RunUntilIdle();

  // All three requests should have been provided to the annotator.
  EXPECT_THAT(test_annotator.source_ids_,
              ElementsAre("test.jpg", "test.jpg", "example.png"));
  ASSERT_THAT(test_annotator.image_processors_, SizeIs(3));
  EXPECT_THAT(test_annotator.image_processors_[0].is_bound(), Eq(true));
  EXPECT_THAT(test_annotator.image_processors_[1].is_bound(), Eq(true));
  EXPECT_THAT(test_annotator.image_processors_[2].is_bound(), Eq(true));
  EXPECT_THAT(test_annotator.callbacks_, SizeIs(3));

  // Image 1 goes away.
  page_annotator.ImageRemoved(1ul);
  test_task_env.RunUntilIdle();

  // The corresponding image processors should have been disconnected.
  ASSERT_THAT(test_annotator.image_processors_, SizeIs(3));
  EXPECT_THAT(test_annotator.image_processors_[0].is_bound(), Eq(false));
  EXPECT_THAT(test_annotator.image_processors_[1].is_bound(), Eq(false));
  EXPECT_THAT(test_annotator.image_processors_[2].is_bound(), Eq(true));

  // Expect success and failure to be reported.
  const auto error = ia_mojom::AnnotateImageResult::NewErrorCode(
      ia_mojom::AnnotateImageError::kCanceled);

  // Can't use an initializer list since it performs copies.
  std::vector<ia_mojom::AnnotationPtr> annotations;
  annotations.push_back(ia_mojom::Annotation::New(
      ia_mojom::AnnotationType::kOcr, 1.0, "text from image"));
  const auto success =
      ia_mojom::AnnotateImageResult::NewAnnotations(std::move(annotations));

  ASSERT_THAT(test_annotator.callbacks_, SizeIs(3));
  std::move(test_annotator.callbacks_[0]).Run(error.Clone());
  std::move(test_annotator.callbacks_[1]).Run(error.Clone());
  std::move(test_annotator.callbacks_[2]).Run(success.Clone());

  EXPECT_CALL(o1, OnImageAnnotated(1ul, IsAnnotateImageResult(error.get())));
  EXPECT_CALL(o2, OnImageAnnotated(1ul, IsAnnotateImageResult(error.get())));
  EXPECT_CALL(o2, OnImageAnnotated(2ul, IsAnnotateImageResult(success.get())));

  test_task_env.RunUntilIdle();
}

}  // namespace

}  // namespace page_image_annotation
