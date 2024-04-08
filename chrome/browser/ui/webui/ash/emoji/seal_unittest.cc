// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "seal.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrNe;

namespace ash {
namespace {

class FakeSnapperProvider : public manta::SnapperProvider {
 public:
  FakeSnapperProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager,
      manta::MantaStatus fake_status,
      manta::proto::Response fake_response)
      : SnapperProvider(std::move(test_url_loader_factory), identity_manager),
        fake_status_(fake_status),
        fake_response_(fake_response) {}

  void Call(manta::proto::Request& request,
            net::NetworkTrafficAnnotationTag traffic_annotation,
            manta::MantaProtoResponseCallback done_callback) override {
    std::move(done_callback)
        .Run(std::make_unique<manta::proto::Response>(fake_response_),
             fake_status_);
  }

 private:
  manta::MantaStatus fake_status_;
  manta::proto::Response fake_response_;
};

class SealTest : public testing::Test {
 protected:
  std::unique_ptr<SealService> CreateSealService(
      std::unique_ptr<manta::SnapperProvider> snapper_provider) {
    return std::make_unique<SealService>(
        /*receiver=*/seal_service_.BindNewPipeAndPassReceiver(),
        /*snapper_provider=*/std::move(snapper_provider));
  }

  std::unique_ptr<FakeSnapperProvider> CreateSnapperProvider(
      manta::MantaStatus fake_status,
      manta::proto::Response fake_response) {
    return std::make_unique<FakeSnapperProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_.identity_manager(), fake_status, fake_response);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<seal::mojom::SealService> seal_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(SealTest, GetImagesReturnsNotEnabledErrorIfNotInitialized) {
  std::unique_ptr<SealService> service = CreateSealService(nullptr);

  base::test::TestFuture<seal::mojom::Status,
                         std::vector<seal::mojom::ImagePtr>>
      future;
  service->GetImages("cat", future.GetCallback());
  const auto& [status, images] = future.Get();
  EXPECT_EQ(status, seal::mojom::Status::kNotEnabledError);
  EXPECT_THAT(images, IsEmpty());
}

TEST_F(SealTest, GetImagesReturnsUnknownErrorDueToSnapperError) {
  manta::proto::Response fake_response;
  std::unique_ptr<SealService> service =
      CreateSealService(CreateSnapperProvider(
          manta::MantaStatus{.status_code =
                                 manta::MantaStatusCode::kGenericError},
          fake_response));

  base::test::TestFuture<seal::mojom::Status,
                         std::vector<seal::mojom::ImagePtr>>
      future;
  service->GetImages("cat", future.GetCallback());
  const auto& [status, images] = future.Get();
  EXPECT_EQ(status, seal::mojom::Status::kUnknownError);
  EXPECT_THAT(images, IsEmpty());
}

TEST_F(SealTest, GetImagesReturnsOk) {
  manta::proto::Response fake_response;
  for (int i = 0; i < 10; ++i) {
    manta::proto::OutputData& fake_output_data =
        *fake_response.add_output_data();
    manta::proto::Image& image = *fake_output_data.mutable_image();
    image.set_serialized_bytes("[image content]");
  }
  std::unique_ptr<SealService> service =
      CreateSealService(CreateSnapperProvider(
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk},
          fake_response));

  base::test::TestFuture<seal::mojom::Status,
                         std::vector<seal::mojom::ImagePtr>>
      future;
  service->GetImages("cat", future.GetCallback());
  const auto& [status, images] = future.Get();
  EXPECT_EQ(status, seal::mojom::Status::kOk);
  ASSERT_THAT(images, SizeIs(10));
  EXPECT_THAT(
      images,
      Each(Pointee(AllOf(
          Field(&seal::mojom::Image::url, Property(&GURL::spec, StrNe(""))),
          Field(&seal::mojom::Image::dimensions,
                AllOf(Property(&gfx::Size::width, Eq(1024)),
                      Property(&gfx::Size::height, Eq(1024))))))));
}

}  // namespace
}  // namespace ash
