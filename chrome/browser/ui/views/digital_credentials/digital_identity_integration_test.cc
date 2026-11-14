// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/digital_credentials/digital_identity_provider_desktop.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_test_path_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

using UiWarningLevel = content::DigitalIdentityProvider::UiWarningLevel;

// Callback for when a views::Widget is shown. Runs `dialog_shown_callback` if
// the shown views::Widget has `expected_dialog_title`.
void OnDialogShown(base::RepeatingClosure dialog_shown_callback,
                   std::u16string expected_dialog_title,
                   views::Widget* widget) {
  if (widget->widget_delegate()->GetWindowTitle() == expected_dialog_title) {
    dialog_shown_callback.Run();
  }
}

// content::DigitalIdentityProvider which:
// - records the received UiWarningLevel on `Get()`
// - always succeeds immediately
// - offers callback notification when `Get()` is invoked.
class TestDigitalIdentityProvider final
    : public DigitalIdentityProviderDesktop {
 public:
  explicit TestDigitalIdentityProvider(
      base::RepeatingCallback<void(UiWarningLevel)> credential_request_observer)
      : credential_request_observer_(std::move(credential_request_observer)) {}
  ~TestDigitalIdentityProvider() override = default;

  void Get(content::WebContents* web_contents,
           const url::Origin& origin,
           base::ValueView request,
           UiWarningLevel warning_level,
           DigitalIdentityCallback callback) override {
    auto observer = credential_request_observer_;
    // Calling the callback might destroy `this`.
    std::move(callback).Run(DigitalCredential(
        "openid4vp",
        base::JSONReader::Read(R"({"token" : "test token"})",
                               base::JSON_PARSE_CHROMIUM_EXTENSIONS)
            .value()));
    if (observer) {
      observer.Run(warning_level);
    }
  }

 private:
  base::RepeatingCallback<void(UiWarningLevel)> credential_request_observer_;
};

// ChromeContentBrowserClient which returns custom
// content::DigitalIdentityProvider.
class TestBrowserClient : public ChromeContentBrowserClient {
 public:
  explicit TestBrowserClient(
      base::RepeatingCallback<void(UiWarningLevel)> credential_request_observer)
      : credential_request_observer_(std::move(credential_request_observer)) {}
  ~TestBrowserClient() override = default;

  std::unique_ptr<content::DigitalIdentityProvider>
  CreateDigitalIdentityProvider() override {
    return std::make_unique<TestDigitalIdentityProvider>(
        credential_request_observer_);
  }

 private:
  base::RepeatingCallback<void(UiWarningLevel)> credential_request_observer_;
};

}  // anonymous namespace

class DigitalIdentityIntegrationTest : public InProcessBrowserTest {
 public:
  DigitalIdentityIntegrationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebIdentityDigitalCredentials);
  }
  ~DigitalIdentityIntegrationTest() override = default;

  void SetUpOnMainThread() override {
    client_ = std::make_unique<TestBrowserClient>(base::BindRepeating(
        &DigitalIdentityIntegrationTest::OnDigitalCredentialRequested,
        weak_ptr_factory_.GetWeakPtr()));
    original_client_ = content::SetBrowserClientForTesting(client_.get());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    content::SetBrowserClientForTesting(original_client_.get());
  }

  // Waits till DigitalIdentityProvider requests credential and returns the
  // computed UiWarningLevel passed to `Get()`.
  UiWarningLevel WaitTillRequestCredential() {
    if (!last_warning_level_.has_value()) {
      base::RunLoop run_loop;
      credential_request_observer_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return *last_warning_level_;
  }

 protected:
  void OnDigitalCredentialRequested(UiWarningLevel warning_level) {
    last_warning_level_ = warning_level;

    if (credential_request_observer_) {
      std::move(credential_request_observer_).Run();
    }
  }

  std::unique_ptr<TestBrowserClient> client_;
  raw_ptr<content::ContentBrowserClient> original_client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<UiWarningLevel> last_warning_level_;
  base::OnceClosure credential_request_observer_;

  base::WeakPtrFactory<DigitalIdentityIntegrationTest> weak_ptr_factory_{this};
};

// Test that an interstitial is shown if the page requests more than just age,
// and accepting it forwards `UiWarningLevel::kHintCaution` to `Get()`.
IN_PROC_BROWSER_TEST_F(DigitalIdentityIntegrationTest,
                       InterstitialShownMoreThanJustAge) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  std::u16string kExpectedDialogTitle = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_DIALOG_TITLE);

  base::RunLoop run_loop;

  auto dialog_observer = std::make_unique<views::AnyWidgetObserver>(
      views::test::AnyWidgetTestPasskey());
  dialog_observer->set_shown_callback(base::BindRepeating(
      [](base::RepeatingClosure quit_closure,
         std::u16string expected_dialog_title, views::Widget* widget) {
        if (widget->widget_delegate()->GetWindowTitle() ==
            expected_dialog_title) {
          quit_closure.Run();
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](views::Widget* w) {
                    w->widget_delegate()->AsDialogDelegate()->AcceptDialog();
                  },
                  widget));
        }
      },
      run_loop.QuitClosure(), kExpectedDialogTitle));

  GURL url = chrome_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("digital_credentials.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      "document.getElementById('request_age_and_name_button').click();"));

  run_loop.Run();
  EXPECT_EQ(UiWarningLevel::kHintCaution, WaitTillRequestCredential());
}

// Test that an interstitial is not shown if the page only requests age, and
// `UiWarningLevel::kHintNoIssues` is forwarded to `Get()`.
IN_PROC_BROWSER_TEST_F(DigitalIdentityIntegrationTest, InterstitialNotShown) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  std::u16string kExpectedDialogTitle = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_DIALOG_TITLE);

  auto dialog_observer = std::make_unique<views::AnyWidgetObserver>(
      views::test::AnyWidgetTestPasskey());
  bool was_dialog_shown = false;

  auto dialog_shown_action = [](bool& was_dialog_shown) {
    was_dialog_shown = true;
  };

  dialog_observer->set_shown_callback(base::BindRepeating(
      &OnDialogShown,
      base::BindRepeating(dialog_shown_action, std::ref(was_dialog_shown)),
      kExpectedDialogTitle));

  GURL url = chrome_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("digital_credentials.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      "document.getElementById('request_age_only_button').click();"));

  EXPECT_EQ(UiWarningLevel::kHintNoIssues, WaitTillRequestCredential());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(was_dialog_shown);
}

class TestQrCodeDigitalIdentityProvider
    : public DigitalIdentityProviderDesktop {
 public:
  explicit TestQrCodeDigitalIdentityProvider(
      base::RepeatingCallback<void(UiWarningLevel)> qr_dialog_observer)
      : qr_dialog_observer_(std::move(qr_dialog_observer)) {}
  ~TestQrCodeDigitalIdentityProvider() override = default;

  void ShowQrCodeDialog(const std::string& qr_url,
                        RequestInfo::RequestType request_type,
                        UiWarningLevel warning_level) override {
    if (qr_dialog_observer_) {
      qr_dialog_observer_.Run(warning_level);
    }
    DigitalIdentityProviderDesktop::ShowQrCodeDialog(qr_url, request_type,
                                                     warning_level);
  }

 private:
  base::RepeatingCallback<void(UiWarningLevel)> qr_dialog_observer_;
};

class QrCodeTestBrowserClient : public ChromeContentBrowserClient {
 public:
  explicit QrCodeTestBrowserClient(
      base::RepeatingCallback<void(UiWarningLevel)> qr_dialog_observer)
      : qr_dialog_observer_(std::move(qr_dialog_observer)) {}
  ~QrCodeTestBrowserClient() override = default;

  std::unique_ptr<content::DigitalIdentityProvider>
  CreateDigitalIdentityProvider() override {
    return std::make_unique<TestQrCodeDigitalIdentityProvider>(
        qr_dialog_observer_);
  }

 private:
  base::RepeatingCallback<void(UiWarningLevel)> qr_dialog_observer_;
};

class DigitalIdentityIntegrationQrCodeTest : public InProcessBrowserTest {
 public:
  DigitalIdentityIntegrationQrCodeTest() = default;
  ~DigitalIdentityIntegrationQrCodeTest() override = default;

  void SetUpOnMainThread() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, IsPresent()).WillByDefault(testing::Return(true));
    ON_CALL(*mock_adapter_, IsPowered()).WillByDefault(testing::Return(true));
    ON_CALL(*mock_adapter_, StartScanWithFilter_(testing::_, testing::_))
        .WillByDefault(testing::WithArg<1>([](auto& callback) {
          std::move(callback).Run(
              /*is_error=*/false,
              device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
        }));
    ON_CALL(*mock_adapter_, StopScan(testing::_))
        .WillByDefault(testing::WithArg<0>([](auto callback) {
          std::move(callback).Run(
              /*is_error=*/false,
              device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
        }));
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
    bluetooth_values_for_testing_ =
        device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
    bluetooth_values_for_testing_->SetLESupported(true);

    client_ = std::make_unique<QrCodeTestBrowserClient>(base::BindRepeating(
        &DigitalIdentityIntegrationQrCodeTest::OnQrCodeDialogShown,
        base::Unretained(this)));
    original_client_ = content::SetBrowserClientForTesting(client_.get());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    content::SetBrowserClientForTesting(original_client_.get());
    bluetooth_values_for_testing_.reset();
    mock_adapter_.reset();
  }

 protected:
  void OnQrCodeDialogShown(UiWarningLevel warning_level) {
    last_qr_dialog_warning_level_ = warning_level;
  }

  std::optional<UiWarningLevel> last_qr_dialog_warning_level() const {
    return last_qr_dialog_warning_level_;
  }

  std::unique_ptr<QrCodeTestBrowserClient> client_;
  raw_ptr<content::ContentBrowserClient> original_client_;

 private:
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalOverrideValues>
      bluetooth_values_for_testing_;
  std::optional<UiWarningLevel> last_qr_dialog_warning_level_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kWebIdentityDigitalCredentials};
};

IN_PROC_BROWSER_TEST_F(DigitalIdentityIntegrationQrCodeTest,
                       QrCodeDialogReceivesHintNoIssuesAndCancelsCleanly) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  std::u16string kExpectedQrTitle =
      l10n_util::GetStringUTF16(IDS_WEB_DIGITAL_CREDENTIALS_QR_TITLE);

  auto dialog_observer = std::make_unique<views::AnyWidgetObserver>(
      views::test::AnyWidgetTestPasskey());

  dialog_observer->set_shown_callback(base::BindRepeating(
      [](std::u16string expected_qr_title, views::Widget* widget) {
        if (widget->widget_delegate()->GetWindowTitle() == expected_qr_title) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](views::Widget* w) {
                    w->CloseWithReason(
                        views::Widget::ClosedReason::kCancelButtonClicked);
                  },
                  widget));
        }
      },
      kExpectedQrTitle));

  GURL url = chrome_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("digital_credentials.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  std::string script = R"(
    (async () => {
      try {
        await navigator.credentials.get({
          digital: {
            requests: [{
              protocol: "openid4vp",
              data: {
                dcql_query: {
                  credentials: [
                    {
                      id: "cred1",
                      format: "mso_mdoc",
                      meta: {
                        doctype_value: "org.iso.18013.5.1.mDL"
                      },
                      claims: [
                        {
                          path: ["org.iso.18013.5.1", "age_over_21"]
                        }
                      ]
                    }
                  ]
                }
              }
            }]
          }
        });
        return "Success";
      } catch (e) {
        return e.name;
      }
    })();
  )";

  EXPECT_EQ("NotAllowedError",
            content::EvalJs(web_contents, script).ExtractString());
  EXPECT_EQ(UiWarningLevel::kHintNoIssues, last_qr_dialog_warning_level());
}

IN_PROC_BROWSER_TEST_F(DigitalIdentityIntegrationQrCodeTest,
                       QrCodeDialogReceivesHintCautionForSensitiveAttributes) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  std::u16string kExpectedInterstitialTitle = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_DIALOG_TITLE);
  std::u16string kExpectedQrTitle =
      l10n_util::GetStringUTF16(IDS_WEB_DIGITAL_CREDENTIALS_QR_TITLE);

  auto dialog_observer = std::make_unique<views::AnyWidgetObserver>(
      views::test::AnyWidgetTestPasskey());

  dialog_observer->set_shown_callback(base::BindRepeating(
      [](std::u16string expected_interstitial_title,
         std::u16string expected_qr_title, views::Widget* widget) {
        std::u16string title = widget->widget_delegate()->GetWindowTitle();
        if (title == expected_interstitial_title) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](views::Widget* w) {
                    w->widget_delegate()->AsDialogDelegate()->AcceptDialog();
                  },
                  widget));
        } else if (title == expected_qr_title) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](views::Widget* w) {
                    w->CloseWithReason(
                        views::Widget::ClosedReason::kCancelButtonClicked);
                  },
                  widget));
        }
      },
      kExpectedInterstitialTitle, kExpectedQrTitle));

  GURL url = chrome_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("digital_credentials.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  std::string script = R"(
    (async () => {
      try {
        await navigator.credentials.get({
          digital: {
            requests: [{
              protocol: "openid4vp",
              data: {
                dcql_query: {
                  credentials: [
                    {
                      id: "cred1",
                      format: "mso_mdoc",
                      meta: {
                        doctype_value: "org.iso.18013.5.1.mDL"
                      },
                      claims: [
                        {
                          path: ["org.iso.18013.5.1", "given_name"]
                        }
                      ]
                    }
                  ]
                }
              }
            }]
          }
        });
        return "Success";
      } catch (e) {
        return e.name;
      }
    })();
  )";

  EXPECT_EQ("NotAllowedError",
            content::EvalJs(web_contents, script).ExtractString());
  EXPECT_EQ(UiWarningLevel::kHintCaution, last_qr_dialog_warning_level());
}
