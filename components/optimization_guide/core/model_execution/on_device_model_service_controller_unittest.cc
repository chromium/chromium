// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/on_device_model_stream_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class FakeOnDeviceModel : public on_device_model::mojom::OnDeviceModel {
  // on_device_model::mojom::OnDeviceModel:
  void Execute(const std::string& input,
               mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
                   response) override {
    mojo::Remote<on_device_model::mojom::StreamingResponder> remote(
        std::move(response));
    remote->OnResponse("Model starting\n");
    remote->OnResponse("Input: " + input + "\n");
    remote->OnComplete();
  }
};

class FakeOnDeviceModelService
    : public on_device_model::mojom::OnDeviceModelService {
 private:
  // on_device_model::mojom::OnDeviceModelService:
  void LoadModel(on_device_model::ModelAssets assets,
                 LoadModelCallback callback) override {
    mojo::PendingRemote<on_device_model::mojom::OnDeviceModel> remote;
    auto test_model = std::make_unique<FakeOnDeviceModel>();
    model_receivers_.Add(std::move(test_model),
                         remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(
        on_device_model::mojom::LoadModelResult::NewModel(std::move(remote)));
  }
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override {
    std::move(callback).Run(
        on_device_model::mojom::PerformanceClass::kVeryHigh);
  }
  mojo::UniqueReceiverSet<on_device_model::mojom::OnDeviceModel>
      model_receivers_;
};

class FakeOnDeviceModelServiceController
    : public OnDeviceModelServiceController {
 private:
  void LaunchService() override {
    mojo::PendingRemote<on_device_model::mojom::OnDeviceModelService>
        pending_remote{receiver_.BindNewPipeAndPassRemote()};
    service_remote_.Bind(std::move(pending_remote));
  }

  FakeOnDeviceModelService service_;
  mojo::Receiver<on_device_model::mojom::OnDeviceModelService> receiver_{
      &service_};
};

class OnDeviceModelServiceControllerTest : public testing::Test {
 public:
  void SetUp() override {
    test_receiver_ =
        std::make_unique<OnDeviceModelStreamReceiver>(base::BindOnce(
            &OnDeviceModelServiceControllerTest::OnStreamReceiverComplete,
            base::Unretained(this)));
    test_controller_.Init(base::FilePath::FromASCII("/foo"));
  }
  void ExecuteModel(std::string_view input) {
    test_controller_.Execute(input, test_receiver_->BindNewPipeAndPassRemote());
  }

 protected:
  void OnStreamReceiverComplete(std::string_view response) {
    stream_response_received_ = response;
  }

  base::test::TaskEnvironment task_environment_;
  FakeOnDeviceModelServiceController test_controller_;
  std::unique_ptr<OnDeviceModelStreamReceiver> test_receiver_;
  absl::optional<std::string> stream_response_received_;
};

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionSuccess) {
  ExecuteModel("foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(stream_response_received_);
  EXPECT_THAT(*stream_response_received_, testing::HasSubstr("Input: foo"));
}

}  // namespace optimization_guide
