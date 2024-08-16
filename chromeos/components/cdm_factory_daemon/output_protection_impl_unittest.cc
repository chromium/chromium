// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/components/cdm_factory_daemon/output_protection_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chromeos/components/cdm_factory_daemon/mojom/output_protection.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/test/fake_display_snapshot.h"

using chromeos::cdm::mojom::OutputProtection;
using testing::_;
using testing::Return;
using testing::ReturnRef;

constexpr uint64_t kFakeClientId = 1;
constexpr int64_t kDisplayIds[] = {123, 234, 345, 456};
const display::DisplayMode kDisplayMode({1366, 768}, false, 60.0f);

namespace chromeos {

namespace {

class MockDisplaySystemDelegate
    : public OutputProtectionImpl::DisplaySystemDelegate {
 public:
  MockDisplaySystemDelegate() = default;
  ~MockDisplaySystemDelegate() override = default;

  MOCK_METHOD(
      void,
      ApplyContentProtection,
      (display::ContentProtectionManager::ClientId,
       int64_t,
       uint32_t,
       display::ContentProtectionManager::ApplyContentProtectionCallback));
  MOCK_METHOD(
      void,
      QueryContentProtection,
      (display::ContentProtectionManager::ClientId,
       int64_t,
       display::ContentProtectionManager::QueryContentProtectionCallback));
  MOCK_METHOD(display::ContentProtectionManager::ClientId, RegisterClient, ());
  MOCK_METHOD(void,
              UnregisterClient,
              (display::ContentProtectionManager::ClientId));
  MOCK_METHOD((const std::vector<
                  raw_ptr<display::DisplaySnapshot, VectorExperimental>>&),
              cached_displays,
              (),
              (const));
};

}  // namespace

class OutputProtectionImplTest : public testing::Test {
 protected:
  OutputProtectionImplTest() {
    std::unique_ptr<MockDisplaySystemDelegate> delegate =
        std::make_unique<MockDisplaySystemDelegate>();
    delegate_ = delegate.get();
    OutputProtectionImpl::Create(
        output_protection_mojo_.BindNewPipeAndPassReceiver(),
        std::move(delegate));
    task_environment_.RunUntilIdle();

    display::DisplayConnectionType conn_types[] = {
        display::DISPLAY_CONNECTION_TYPE_INTERNAL,
        display::DISPLAY_CONNECTION_TYPE_HDMI,
        display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT,
        display::DISPLAY_CONNECTION_TYPE_VGA};
    for (size_t i = 0; i < std::size(kDisplayIds); ++i) {
      displays_[i] = display::FakeDisplaySnapshot::Builder()
                         .SetId(kDisplayIds[i])
                         .SetType(conn_types[i])
                         .SetCurrentMode(kDisplayMode.Clone())
                         .Build();
    }

    UpdateDisplays(2);

    EXPECT_CALL(*delegate_, RegisterClient())
        .WillOnce(Return(std::optional<uint64_t>(kFakeClientId)));
  }

  void UpdateDisplays(size_t count) {
    ASSERT_LE(count, std::size(displays_));

    cached_displays_.clear();
    for (size_t i = 0; i < count; ++i)
      cached_displays_.push_back(displays_[i].get());
  }

  ~OutputProtectionImplTest() override {
    EXPECT_CALL(*delegate_, UnregisterClient(_));
    output_protection_mojo_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void ExpectProtectionCall(int64_t display_id,
                            display::ContentProtectionMethod method,
                            bool ret) {
    EXPECT_CALL(*delegate_, ApplyContentProtection(_, display_id, method, _))
        .WillOnce([ret](display::ContentProtectionManager::ClientId client_id,
                        int64_t, uint32_t,
                        display::ContentProtectionManager::
                            ApplyContentProtectionCallback callback) {
          EXPECT_EQ(*client_id, kFakeClientId);
          std::move(callback).Run(ret);
        });
  }

  void ExpectQueryCall(int64_t display_id,
                       uint32_t connection_mask,
                       display::ContentProtectionMethod method,
                       bool ret) {
    EXPECT_CALL(*delegate_, QueryContentProtection(_, display_id, _))
        .WillOnce([connection_mask, method, ret](
                      display::ContentProtectionManager::ClientId client_id,
                      int64_t,
                      display::ContentProtectionManager::
                          QueryContentProtectionCallback callback) {
          EXPECT_EQ(*client_id, kFakeClientId);
          std::move(callback).Run(ret, connection_mask, method);
        });
  }

  mojo::Remote<OutputProtection> output_protection_mojo_;
  raw_ptr<MockDisplaySystemDelegate, AcrossTasksDanglingUntriaged>
      delegate_;  // Not owned.
  std::unique_ptr<display::DisplaySnapshot> displays_[std::size(kDisplayIds)];
  std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>>
      cached_displays_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(OutputProtectionImplTest, ApplyToNoDisplays) {
  UpdateDisplays(0);
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  EXPECT_CALL(*delegate_, ApplyContentProtection(_, _, _, _)).Times(0);
  base::MockCallback<cdm::mojom::OutputProtection::EnableProtectionCallback>
      callback_mock;
  EXPECT_CALL(callback_mock, Run(true));
  output_protection_mojo_->EnableProtection(
      OutputProtection::ProtectionType::HDCP_TYPE_0, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OutputProtectionImplTest, ApplyToMultipleDisplays) {
  UpdateDisplays(4);
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  for (int i = 0; i < 4; i++)
    ExpectProtectionCall(kDisplayIds[i],
                         display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0, true);

  base::MockCallback<cdm::mojom::OutputProtection::EnableProtectionCallback>
      callback_mock;
  EXPECT_CALL(callback_mock, Run(true));
  output_protection_mojo_->EnableProtection(
      OutputProtection::ProtectionType::HDCP_TYPE_0, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OutputProtectionImplTest, ApplyToMultipleDisplaysOneFails) {
  UpdateDisplays(4);
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  for (int i = 0; i < 4; i++) {
    ExpectProtectionCall(
        kDisplayIds[i], display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0, i != 2);
  }
  base::MockCallback<cdm::mojom::OutputProtection::EnableProtectionCallback>
      callback_mock;
  EXPECT_CALL(callback_mock, Run(false));
  output_protection_mojo_->EnableProtection(
      OutputProtection::ProtectionType::HDCP_TYPE_0, callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OutputProtectionImplTest, ApplyDoesNotAggregateTypes) {
  UpdateDisplays(1);
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  OutputProtection::ProtectionType applied_types[] = {
      OutputProtection::ProtectionType::HDCP_TYPE_0,
      OutputProtection::ProtectionType::HDCP_TYPE_1,
      OutputProtection::ProtectionType::NONE};
  display::ContentProtectionMethod expected_types[] = {
      display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0,
      display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1,
      display::CONTENT_PROTECTION_METHOD_NONE};

  for (size_t i = 0; i < std::size(applied_types); ++i) {
    ExpectProtectionCall(kDisplayIds[0], expected_types[i], true);

    base::MockCallback<OutputProtection::EnableProtectionCallback>
        callback_mock;
    EXPECT_CALL(callback_mock, Run(true));
    output_protection_mojo_->EnableProtection(applied_types[i],
                                              callback_mock.Get());
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(OutputProtectionImplTest, QueryNoDisplays) {
  UpdateDisplays(0);
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  EXPECT_CALL(*delegate_, QueryContentProtection(_, _, _)).Times(0);
  base::MockCallback<cdm::mojom::OutputProtection::QueryStatusCallback>
      callback_mock;
  EXPECT_CALL(callback_mock,
              Run(true, 0, OutputProtection::ProtectionType::NONE));
  output_protection_mojo_->QueryStatus(callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OutputProtectionImplTest, QueryInternalOnly) {
  UpdateDisplays(1);
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  ExpectQueryCall(kDisplayIds[0], display::DISPLAY_CONNECTION_TYPE_INTERNAL,
                  display::CONTENT_PROTECTION_METHOD_NONE, true);
  base::MockCallback<cdm::mojom::OutputProtection::QueryStatusCallback>
      callback_mock;
  EXPECT_CALL(callback_mock,
              Run(true, display::DISPLAY_CONNECTION_TYPE_INTERNAL,
                  OutputProtection::ProtectionType::NONE));
  output_protection_mojo_->QueryStatus(callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OutputProtectionImplTest, QueryInternalExternalType0) {
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  ExpectQueryCall(kDisplayIds[0], display::DISPLAY_CONNECTION_TYPE_INTERNAL,
                  display::CONTENT_PROTECTION_METHOD_NONE, true);
  ExpectQueryCall(kDisplayIds[1], display::DISPLAY_CONNECTION_TYPE_HDMI,
                  display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0, true);
  base::MockCallback<cdm::mojom::OutputProtection::QueryStatusCallback>
      callback_mock;
  EXPECT_CALL(callback_mock,
              Run(true,
                  display::DISPLAY_CONNECTION_TYPE_INTERNAL |
                      display::DISPLAY_CONNECTION_TYPE_HDMI,
                  OutputProtection::ProtectionType::HDCP_TYPE_0));
  output_protection_mojo_->QueryStatus(callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OutputProtectionImplTest, QueryInternalExternalType1) {
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  ExpectQueryCall(kDisplayIds[0], display::DISPLAY_CONNECTION_TYPE_INTERNAL,
                  display::CONTENT_PROTECTION_METHOD_NONE, true);
  ExpectQueryCall(kDisplayIds[1], display::DISPLAY_CONNECTION_TYPE_HDMI,
                  display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1, true);
  base::MockCallback<cdm::mojom::OutputProtection::QueryStatusCallback>
      callback_mock;
  EXPECT_CALL(callback_mock,
              Run(true,
                  display::DISPLAY_CONNECTION_TYPE_INTERNAL |
                      display::DISPLAY_CONNECTION_TYPE_HDMI,
                  OutputProtection::ProtectionType::HDCP_TYPE_1));
  output_protection_mojo_->QueryStatus(callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OutputProtectionImplTest, QueryInternalMultiExternalMultiType) {
  UpdateDisplays(3);
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  ExpectQueryCall(kDisplayIds[0], display::DISPLAY_CONNECTION_TYPE_INTERNAL,
                  display::CONTENT_PROTECTION_METHOD_NONE, true);
  ExpectQueryCall(kDisplayIds[1], display::DISPLAY_CONNECTION_TYPE_HDMI,
                  display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1, true);
  ExpectQueryCall(kDisplayIds[2], display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT,
                  display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0, true);
  base::MockCallback<cdm::mojom::OutputProtection::QueryStatusCallback>
      callback_mock;
  EXPECT_CALL(callback_mock,
              Run(true,
                  display::DISPLAY_CONNECTION_TYPE_INTERNAL |
                      display::DISPLAY_CONNECTION_TYPE_HDMI |
                      display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT,
                  OutputProtection::ProtectionType::HDCP_TYPE_0));
  output_protection_mojo_->QueryStatus(callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OutputProtectionImplTest, QueryAnalog) {
  UpdateDisplays(4);
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  ExpectQueryCall(kDisplayIds[0], display::DISPLAY_CONNECTION_TYPE_INTERNAL,
                  display::CONTENT_PROTECTION_METHOD_NONE, true);
  ExpectQueryCall(kDisplayIds[1], display::DISPLAY_CONNECTION_TYPE_HDMI,
                  display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1, true);
  ExpectQueryCall(kDisplayIds[2], display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT,
                  display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0, true);
  ExpectQueryCall(kDisplayIds[3], display::DISPLAY_CONNECTION_TYPE_VGA,
                  display::CONTENT_PROTECTION_METHOD_NONE, true);
  base::MockCallback<cdm::mojom::OutputProtection::QueryStatusCallback>
      callback_mock;
  EXPECT_CALL(callback_mock,
              Run(true,
                  display::DISPLAY_CONNECTION_TYPE_INTERNAL |
                      display::DISPLAY_CONNECTION_TYPE_HDMI |
                      display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT |
                      display::DISPLAY_CONNECTION_TYPE_VGA,
                  OutputProtection::ProtectionType::NONE));
  output_protection_mojo_->QueryStatus(callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(OutputProtectionImplTest, QueryWithFailure) {
  UpdateDisplays(3);
  EXPECT_CALL(*delegate_, cached_displays())
      .WillOnce(ReturnRef(cached_displays_));
  ExpectQueryCall(kDisplayIds[0], display::DISPLAY_CONNECTION_TYPE_INTERNAL,
                  display::CONTENT_PROTECTION_METHOD_NONE, true);
  ExpectQueryCall(kDisplayIds[1], display::DISPLAY_CONNECTION_TYPE_HDMI,
                  display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1, false);
  ExpectQueryCall(kDisplayIds[2], display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT,
                  display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0, true);
  base::MockCallback<cdm::mojom::OutputProtection::QueryStatusCallback>
      callback_mock;
  EXPECT_CALL(callback_mock, Run(false, _, _));
  output_protection_mojo_->QueryStatus(callback_mock.Get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
