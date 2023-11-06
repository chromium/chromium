// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "build/buildflag.h"
#include "third_party/mediapipe/buildflags.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator.pb.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_graph.h"
#include "third_party/mediapipe/src/mediapipe/framework/formats/image_frame.h"

namespace {

[[maybe_unused]] uint8_t GetPixelValue(int x, int y, int iteration) {
  return (1 + 3 * x + 5 * y + 2 * iteration) % 256;
}

TEST(MediaEffectsMediaPipe, OnCpu) {
  // Configures a simple graph, which concatenates 2 PassThroughCalculators.

  mediapipe::CalculatorGraphConfig config;
  config.add_input_stream("in");
  config.add_output_stream("out");

  auto* node1 = config.add_node();
  *node1->mutable_calculator() = "PassThroughCalculator";
  node1->add_input_stream("in");
  node1->add_output_stream("out1");

  auto* node2 = config.add_node();
  *node2->mutable_calculator() = "PassThroughCalculator";
  node2->add_input_stream("out1");
  node2->add_output_stream("out");

  mediapipe::CalculatorGraph graph;
  absl::Status status = graph.Initialize(config);
  ASSERT_TRUE(status.ok()) << "Failed to initialize calculator graph, error: "
                           << status;

  absl::StatusOr<mediapipe::OutputStreamPoller> maybe_poller =
      graph.AddOutputStreamPoller("out");
  ASSERT_TRUE(maybe_poller.ok())
      << "Failed to obtain poller, error: " << maybe_poller.status();

  mediapipe::OutputStreamPoller poller = std::move(maybe_poller.value());

  status = graph.StartRun({});
  ASSERT_TRUE(status.ok()) << "Failed to start run on the graph, error: "
                           << status;

  // Give 10 input packets that contains the same string "Hello World!".
  for (int i = 0; i < 10; ++i) {
    status = graph.AddPacketToInputStream(
        "in", mediapipe::MakePacket<std::string>("Hello World!")
                  .At(mediapipe::Timestamp(i)));

    ASSERT_TRUE(status.ok())
        << "Failed to add packet to input stream, error: " << status;
  }

  // Close the input stream "in".
  status = graph.CloseInputStream("in");
  ASSERT_TRUE(status.ok()) << "Failed to close the input stream, error: "
                           << status;

  int iteration = 0;
  mediapipe::Packet packet;
  // Get the output packets string.
  while (poller.Next(&packet)) {
    LOG(INFO) << packet.Get<std::string>();
    ++iteration;
  }

  EXPECT_EQ(iteration, 10)
      << "insufficient number of packets read from the graph!";

  status = graph.WaitUntilDone();
  ASSERT_TRUE(status.ok()) << "WaitUntilDone() failed, error: " << status;
}

#if BUILDFLAG(MEDIAPIPE_BUILD_WITH_GPU_SUPPORT)
TEST(MediaEffectsMediaPipe, OnGpu) {
  // Configures a simple graph, which sends an on-CPU frame to GPU and then
  // reads back from it.

  mediapipe::CalculatorGraphConfig config;
  config.add_input_stream("in");
  config.add_output_stream("out");

  auto* node1 = config.add_node();
  *node1->mutable_calculator() = "ImageFrameToGpuBufferCalculator";
  node1->add_input_stream("in");
  node1->add_output_stream("out1");

  auto* node2 = config.add_node();
  *node2->mutable_calculator() = "GpuBufferToImageFrameCalculator";
  node2->add_input_stream("out1");
  node2->add_output_stream("out");

  mediapipe::CalculatorGraph graph;
  absl::Status status = graph.Initialize(config);
  ASSERT_TRUE(status.ok()) << "Failed to initialize calculator graph, error: "
                           << status;

  absl::StatusOr<mediapipe::OutputStreamPoller> maybe_poller =
      graph.AddOutputStreamPoller("out");
  ASSERT_TRUE(maybe_poller.ok())
      << "Failed to obtain poller, error: " << maybe_poller.status();

  mediapipe::OutputStreamPoller poller = std::move(maybe_poller.value());

  status = graph.StartRun({});
  ASSERT_TRUE(status.ok()) << "Failed to start run on the graph, error: "
                           << status;

  for (int i = 0; i < 10; ++i) {
    mediapipe::ImageFrame input(mediapipe::ImageFormat::SRGBA, 10, 10);

    uint8_t* pixel_data = input.MutablePixelData();
    for (int col = 0; col < input.Width(); ++col) {
      for (int row = 0; row < input.Height(); ++row) {
        *(pixel_data + row + col * input.WidthStep()) =
            GetPixelValue(col, row, i);
      }
    }

    status = graph.AddPacketToInputStream(
        "in", mediapipe::MakePacket<mediapipe::ImageFrame>(std::move(input))
                  .At(mediapipe::Timestamp(i)));
    ASSERT_TRUE(status.ok())
        << "Failed to add packet to input stream, error: " << status;
  }

  // Close the input stream "in".
  status = graph.CloseInputStream("in");
  ASSERT_TRUE(status.ok()) << "Failed to close the input stream, error: "
                           << status;

  int iteration = 0;
  mediapipe::Packet packet;
  // Get the output packets string.
  while (poller.Next(&packet)) {
    const mediapipe::ImageFrame& output = packet.Get<mediapipe::ImageFrame>();

    const uint8_t* pixel_data = output.PixelData();
    for (int col = 0; col < output.Width(); ++col) {
      for (int row = 0; row < output.Height(); ++row) {
        const uint8_t expected = GetPixelValue(col, row, iteration);
        const uint8_t got = *(pixel_data + row + col * output.WidthStep());

        EXPECT_EQ(got, expected)
            << "data mismatch at row " << row << ", column " << col
            << ", iteration " << iteration << ", expected " << expected
            << ", got " << got;
      }
    }

    ++iteration;
  }

  EXPECT_EQ(iteration, 10)
      << "insufficient number of packets read from the graph!";

  status = graph.WaitUntilDone();
  ASSERT_TRUE(status.ok()) << "WaitUntilDone() failed, error: " << status;
}

#endif

}  // namespace
