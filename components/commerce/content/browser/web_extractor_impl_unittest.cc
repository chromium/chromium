// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/browser/web_extractor_impl.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/commerce/content/browser/web_contents_wrapper.h"
#include "components/commerce/core/mojom/commerce_web_extractor.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace commerce {

class WebExtractorImplTest : public content::RenderViewHostTestHarness {
 public:
  WebExtractorImplTest() = default;
  WebExtractorImplTest(const WebExtractorImplTest&) = delete;
  WebExtractorImplTest& operator=(const WebExtractorImplTest&) = delete;
  ~WebExtractorImplTest() override = default;
};

class MockCommerceWebExtractor
    : public commerce_web_extractor::mojom::CommerceWebExtractor {
 public:
  MockCommerceWebExtractor() = default;
  MockCommerceWebExtractor(const MockCommerceWebExtractor&) = delete;
  MockCommerceWebExtractor& operator=(const MockCommerceWebExtractor&) = delete;
  ~MockCommerceWebExtractor() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<
                   commerce_web_extractor::mojom::CommerceWebExtractor>(
        std::move(handle)));
  }

  MOCK_METHOD1(ExtractMetaInfo, void(ExtractMetaInfoCallback callback));

 private:
  mojo::Receiver<commerce_web_extractor::mojom::CommerceWebExtractor> receiver_{
      this};
};

TEST_F(WebExtractorImplTest, TestExtraction) {
  std::unique_ptr<MockCommerceWebExtractor> extractor =
      std::make_unique<MockCommerceWebExtractor>();

  // Set up mock CommerceWebExtractor.
  std::unique_ptr<content::WebContents> wc = CreateTestWebContents();
  content::RenderFrameHostTester::For(wc->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();
  std::unique_ptr<service_manager::InterfaceProvider::TestApi> test_api =
      std::make_unique<service_manager::InterfaceProvider::TestApi>(
          wc->GetPrimaryMainFrame()->GetRemoteInterfaces());
  test_api->SetBinderForName(
      commerce_web_extractor::mojom::CommerceWebExtractor::Name_,
      base::BindRepeating(&MockCommerceWebExtractor::Bind,
                          base::Unretained(extractor.get())));

  // Initialize WebExtractor.
  std::unique_ptr<commerce::WebExtractorImpl> web_extractor =
      std::make_unique<commerce::WebExtractorImpl>();
  std::unique_ptr<commerce::WebContentsWrapper> web_wrapper =
      std::make_unique<commerce::WebContentsWrapper>(wc.get(), 0u);
  EXPECT_CALL(*extractor, ExtractMetaInfo(testing::_))
      .WillOnce(testing::Invoke(
          [](MockCommerceWebExtractor::ExtractMetaInfoCallback callback) {
            std::move(callback).Run(base::Value("123"));
          }));
  base::MockCallback<base::OnceCallback<void(const base::Value)>> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](base::Value value) { ASSERT_EQ(value.GetString(), "123"); }));

  web_extractor->ExtractMetaInfo(web_wrapper.get(), callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(WebExtractorImplTest, TestNullWebContents) {
  std::unique_ptr<content::WebContents> wc = CreateTestWebContents();
  std::unique_ptr<commerce::WebContentsWrapper> web_wrapper =
      std::make_unique<commerce::WebContentsWrapper>(wc.get(), 0u);

  web_wrapper->ClearWebContentsPointer();

  ASSERT_FALSE(web_wrapper->GetPrimaryMainFrame());
}
}  // namespace commerce
