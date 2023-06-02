// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/parallel_download_utils.h"

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_file_impl.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/mock_input_stream.h"
#include "components/download/public/common/parallel_download_configs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::StrictMock;

namespace download {

namespace {

const int kErrorStreamOffset = 100;

}  // namespace

class ParallelDownloadUtilsTest : public testing::Test {};

class ParallelDownloadUtilsRecoverErrorTest
    : public ::testing::TestWithParam<int64_t> {
 public:
  ParallelDownloadUtilsRecoverErrorTest() : input_stream_(nullptr) {}

  // Creates a source stream to test.
  std::unique_ptr<DownloadFileImpl::SourceStream> CreateSourceStream(
      int64_t offset) {
    input_stream_ = new StrictMock<MockInputStream>();
    EXPECT_CALL(*input_stream_, GetCompletionStatus())
        .WillRepeatedly(Return(DOWNLOAD_INTERRUPT_REASON_NONE));
    return std::make_unique<DownloadFileImpl::SourceStream>(
        offset, offset, std::unique_ptr<MockInputStream>(input_stream_));
  }

 protected:
  // Stream for sending data into the SourceStream.
  raw_ptr<StrictMock<MockInputStream>, DanglingUntriaged> input_stream_;
};

TEST_F(ParallelDownloadUtilsTest, FindSlicesToDownload) {
  std::vector<DownloadItem::ReceivedSlice> downloaded_slices;
  std::vector<DownloadItem::ReceivedSlice> slices_to_download =
      FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(0, slices_to_download[0].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  downloaded_slices.emplace_back(0, 500);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(500, slices_to_download[0].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  // Create a gap between slices.
  downloaded_slices.emplace_back(1000, 500);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(2u, slices_to_download.size());
  EXPECT_EQ(500, slices_to_download[0].offset);
  EXPECT_EQ(500, slices_to_download[0].received_bytes);
  EXPECT_EQ(1500, slices_to_download[1].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[1].received_bytes);

  // Fill the gap.
  downloaded_slices.emplace(downloaded_slices.begin() + 1,
                            slices_to_download[0]);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(1500, slices_to_download[0].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  // Create a new gap at the beginning.
  downloaded_slices.erase(downloaded_slices.begin());
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(2u, slices_to_download.size());
  EXPECT_EQ(0, slices_to_download[0].offset);
  EXPECT_EQ(500, slices_to_download[0].received_bytes);
  EXPECT_EQ(1500, slices_to_download[1].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[1].received_bytes);
}

TEST_F(ParallelDownloadUtilsTest, AddOrMergeReceivedSliceIntoSortedArray) {
  std::vector<DownloadItem::ReceivedSlice> slices;
  DownloadItem::ReceivedSlice slice1(500, 500);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice1, slices));
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(slice1, slices[0]);

  // Adding a slice that can be merged with existing slice.
  DownloadItem::ReceivedSlice slice2(1000, 400);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice2, slices));
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(500, slices[0].offset);
  EXPECT_EQ(900, slices[0].received_bytes);

  DownloadItem::ReceivedSlice slice3(0, 50);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice3, slices));
  EXPECT_EQ(2u, slices.size());
  EXPECT_EQ(slice3, slices[0]);

  DownloadItem::ReceivedSlice slice4(100, 50);
  EXPECT_EQ(1u, AddOrMergeReceivedSliceIntoSortedArray(slice4, slices));
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(slice3, slices[0]);
  EXPECT_EQ(slice4, slices[1]);

  // A new slice can only merge with an existing slice earlier in the file, not
  // later in the file.
  DownloadItem::ReceivedSlice slice5(50, 50);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice5, slices));
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(100, slices[0].received_bytes);
  EXPECT_EQ(slice4, slices[1]);
}

// Verify if a preceding stream can recover the download for half open error
// stream(the current last stream).
TEST_P(ParallelDownloadUtilsRecoverErrorTest,
       RecoverErrorForHalfOpenErrorStream) {
  // Create a stream that will work on byte range "100-".
  auto error_stream = CreateSourceStream(kErrorStreamOffset);
  error_stream->set_finished(true);

  // Get starting offset of preceding stream.
  int64_t preceding_offset = GetParam();
  EXPECT_LT(preceding_offset, kErrorStreamOffset);
  auto preceding_stream = CreateSourceStream(preceding_offset);
  // Half open preceding stream can always recover the error for later streams.
  EXPECT_FALSE(preceding_stream->is_finished());
  EXPECT_EQ(0u, preceding_stream->bytes_written());
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Half open finished preceding stream with 0 bytes written, if there is no
  // error, the download should be finished.
  preceding_stream->set_finished(true);
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            preceding_stream->GetCompletionStatus());
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Half open finished preceding stream with error, should be treated as
  // failed.
  EXPECT_CALL(*input_stream_, GetCompletionStatus())
      .WillRepeatedly(Return(DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Even if it has written some data.
  preceding_stream->OnBytesConsumed(1000u, 1000u);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  int64_t bytes_consumed = kErrorStreamOffset - preceding_offset - 1;
  // Half open successfully finished preceding stream should always be
  // able to recover error, even if it is not reaching the error offset as the
  // error stream might be requesting something our of range.
  preceding_stream = CreateSourceStream(preceding_offset);
  preceding_stream->set_finished(false);
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->set_finished(true);
  preceding_stream->OnBytesConsumed(bytes_consumed, bytes_consumed);
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnBytesConsumed(1, 1);
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // If the preceding stream is truncated, it should never be able to recover
  // a half open stream.
  preceding_stream = CreateSourceStream(preceding_offset);
  preceding_stream->TruncateLengthWithWrittenDataBlock(kErrorStreamOffset, 1);
  EXPECT_EQ(preceding_stream->length(), kErrorStreamOffset - preceding_offset);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->set_finished(true);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnBytesConsumed(bytes_consumed, bytes_consumed);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnBytesConsumed(1, 1);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
}

// Verify recovery for length capped error stream.
TEST_P(ParallelDownloadUtilsRecoverErrorTest,
       RecoverErrorForLengthCappedErrorStream) {
  // Create a stream that will work on byte range "100-150".
  const int kErrorStreamLength = 50;
  auto error_stream = CreateSourceStream(kErrorStreamOffset);
  error_stream->TruncateLengthWithWrittenDataBlock(
      kErrorStreamOffset + kErrorStreamLength, 1);
  EXPECT_EQ(error_stream->length(), 50);
  error_stream->set_finished(true);

  // Get starting offset of preceding stream.
  const int64_t preceding_offset = GetParam();
  EXPECT_LT(preceding_offset, kErrorStreamOffset);

  // Create an half open preceding stream.
  auto preceding_stream = CreateSourceStream(preceding_offset);
  EXPECT_FALSE(preceding_stream->is_finished());
  EXPECT_EQ(0u, preceding_stream->bytes_written());

  // Since the preceding stream can reach the starting offset, it should be able
  // to recover the error stream..
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  int64_t bytes_consumed = kErrorStreamOffset - preceding_offset;
  preceding_stream->OnBytesConsumed(bytes_consumed, bytes_consumed);
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnBytesConsumed(kErrorStreamLength - 1,
                                    kErrorStreamLength - 1);
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnBytesConsumed(1, 1);
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // If preceding stream is truncated after error stream, checks data written.
  preceding_stream = CreateSourceStream(preceding_offset);
  preceding_stream->TruncateLengthWithWrittenDataBlock(
      kErrorStreamOffset + kErrorStreamLength, 1);
  EXPECT_EQ(preceding_stream->length(),
            kErrorStreamOffset + kErrorStreamLength - preceding_offset);
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->set_finished(true);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnBytesConsumed(bytes_consumed, bytes_consumed);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnBytesConsumed(kErrorStreamLength - 1,
                                    kErrorStreamLength - 1);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnBytesConsumed(1, 1);
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Even if inject an error, since data written has cover the upper bound of
  // the error stream, it should succeed.
  EXPECT_CALL(*input_stream_, GetCompletionStatus())
      .WillRepeatedly(Return(DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // If preceding stream is truncated before or in the middle of error stream,
  // it should not recover the error stream when it reaches its length.
  preceding_stream = CreateSourceStream(preceding_offset);
  preceding_stream->TruncateLengthWithWrittenDataBlock(kErrorStreamOffset + 1,
                                                       1);
  EXPECT_EQ(preceding_stream->length(),
            kErrorStreamOffset + 1 - preceding_offset);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->set_finished(true);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnBytesConsumed(bytes_consumed + 1, bytes_consumed + 1);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
}

// The testing value specified offset for preceding stream. The error stream
// offset is fixed value.
INSTANTIATE_TEST_SUITE_P(ParallelDownloadUtilsTestSuite,
                         ParallelDownloadUtilsRecoverErrorTest,
                         ::testing::Values(0, 20, 80));

// Ensure the minimum slice size is correctly applied.
TEST_F(ParallelDownloadUtilsTest, FindSlicesForRemainingContentMinSliceSize) {
  // Minimum slice size is smaller than total length, only one slice returned.
  DownloadItem::ReceivedSlices slices =
      FindSlicesForRemainingContent(0, 100, 3, 150);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(0, slices[0].received_bytes);

  // Request count is large, the minimum slice size should limit the number of
  // slices returned.
  slices = FindSlicesForRemainingContent(0, 100, 33, 50);
  EXPECT_EQ(2u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(50, slices[0].received_bytes);
  EXPECT_EQ(50, slices[1].offset);
  EXPECT_EQ(0, slices[1].received_bytes);

  // Can chunk 2 slices under minimum slice size, but request count is only 1,
  // request count should win.
  slices = FindSlicesForRemainingContent(0, 100, 1, 50);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(0, slices[0].received_bytes);

  // A total 100 bytes data and a 51 bytes minimum slice size, only one slice is
  // returned.
  slices = FindSlicesForRemainingContent(0, 100, 3, 51);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(0, slices[0].received_bytes);

  // Extreme case where size is smaller than request number.
  slices = FindSlicesForRemainingContent(0, 1, 3, 1);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(DownloadItem::ReceivedSlice(0, 0), slices[0]);

  // Normal case.
  slices = FindSlicesForRemainingContent(0, 100, 3, 5);
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(DownloadItem::ReceivedSlice(0, 33), slices[0]);
  EXPECT_EQ(DownloadItem::ReceivedSlice(33, 33), slices[1]);
  EXPECT_EQ(DownloadItem::ReceivedSlice(66, 0), slices[2]);
}

TEST_F(ParallelDownloadUtilsTest, GetMaxContiguousDataBlockSizeFromBeginning) {
  std::vector<DownloadItem::ReceivedSlice> slices;
  slices.emplace_back(500, 500);
  EXPECT_EQ(0, GetMaxContiguousDataBlockSizeFromBeginning(slices));

  DownloadItem::ReceivedSlice slice1(0, 200);
  AddOrMergeReceivedSliceIntoSortedArray(slice1, slices);
  EXPECT_EQ(200, GetMaxContiguousDataBlockSizeFromBeginning(slices));

  DownloadItem::ReceivedSlice slice2(200, 300);
  AddOrMergeReceivedSliceIntoSortedArray(slice2, slices);
  EXPECT_EQ(1000, GetMaxContiguousDataBlockSizeFromBeginning(slices));
}

// Test to verify Finch parameters for enabled experiment group is read
// correctly.
TEST_F(ParallelDownloadUtilsTest, FinchConfigEnabled) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> params = {
      {kMinSliceSizeFinchKey, "1234"},
      {kParallelRequestCountFinchKey, "6"},
      {kParallelRequestDelayFinchKey, "2000"},
      {kParallelRequestRemainingTimeFinchKey, "3"}};
  feature_list.InitAndEnableFeatureWithParameters(
      features::kParallelDownloading, params);
  EXPECT_TRUE(IsParallelDownloadEnabled());
  EXPECT_EQ(GetMinSliceSizeConfig(), 1234);
  EXPECT_EQ(GetParallelRequestCountConfig(), 6);
  EXPECT_EQ(GetParallelRequestDelayConfig(), base::Seconds(2));
  EXPECT_EQ(GetParallelRequestRemainingTimeConfig(), base::Seconds(3));
}

// Test to verify the disable experiment group will actually disable the
// feature.
TEST_F(ParallelDownloadUtilsTest, FinchConfigDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kParallelDownloading);
  EXPECT_FALSE(IsParallelDownloadEnabled());
}

// Test to verify that the Finch parameter |enable_parallel_download| works
// correctly.
TEST_F(ParallelDownloadUtilsTest, FinchConfigDisabledWithParameter) {
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {kMinSliceSizeFinchKey, "4321"},
        {kEnableParallelDownloadFinchKey, "false"}};
    feature_list.InitAndEnableFeatureWithParameters(
        features::kParallelDownloading, params);
    // Use |enable_parallel_download| to disable parallel download in enabled
    // experiment group.
    EXPECT_FALSE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {kMinSliceSizeFinchKey, "4321"},
        {kEnableParallelDownloadFinchKey, "true"}};
    feature_list.InitAndEnableFeatureWithParameters(
        features::kParallelDownloading, params);
    // Disable only if |enable_parallel_download| sets to false.
    EXPECT_TRUE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {kMinSliceSizeFinchKey, "4321"}};
    feature_list.InitAndEnableFeatureWithParameters(
        features::kParallelDownloading, params);
    // Empty |enable_parallel_download| in an enabled experiment group will have
    // no impact.
    EXPECT_TRUE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
}

}  // namespace download
