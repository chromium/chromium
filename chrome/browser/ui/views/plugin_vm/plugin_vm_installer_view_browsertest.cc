// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plugin_vm/plugin_vm_installer_view.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/fake_vm_plugin_dispatcher_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/test/ax_event_counter.h"

namespace {

const char kZipFile[] = "/downloads/a_zip_file.zip";
const char kZipFileHash[] =
    "bb077522e6c6fec07cf863ca44d5701935c4bc36ed12ef154f4cc22df70aec18";
const char kNonMatchingHash[] =
    "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32";
const char kJpgFile[] = "/downloads/image.jpg";
const char kJpgFileHash[] =
    "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b";

}  // namespace

// TODO(timloh): This file should only be responsible for testing the
// interactions between the installer UI and the installer backend. We should
// mock out the backend and move the tests for the backend logic out of here.

class PluginVmInstallerViewBrowserTest : public DialogBrowserTest {
 public:
  PluginVmInstallerViewBrowserTest() = default;

  PluginVmInstallerViewBrowserTest(const PluginVmInstallerViewBrowserTest&) =
      delete;
  PluginVmInstallerViewBrowserTest& operator=(
      const PluginVmInstallerViewBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    fake_concierge_client_ = ash::FakeConciergeClient::Get();
    fake_concierge_client_->set_disk_image_progress_signal_connected(true);
    fake_vm_plugin_dispatcher_client_ =
        static_cast<ash::FakeVmPluginDispatcherClient*>(
            ash::VmPluginDispatcherClient::Get());

    network_connection_tracker_ =
        network::TestNetworkConnectionTracker::CreateInstance();
    content::SetNetworkConnectionTrackerForTesting(nullptr);
    content::SetNetworkConnectionTrackerForTesting(
        network_connection_tracker_.get());
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    plugin_vm::ShowPluginVmInstallerView(browser()->profile());
    view_ = PluginVmInstallerView::GetActiveViewForTesting();
  }

 protected:
  bool HasAcceptButton() { return view_->GetOkButton() != nullptr; }

  bool HasCancelButton() { return view_->GetCancelButton() != nullptr; }

  void AllowPluginVm() {
    EnterpriseEnrollDevice();
    SetPluginVmPolicies();
    // Set correct PluginVmImage preference value.
    SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                         kZipFileHash);
    auto* installer = plugin_vm::PluginVmInstallerFactory::GetForProfile(
        browser()->profile());
    installer->SetFreeDiskSpaceForTesting(installer->RequiredFreeDiskSpace());
    installer->SkipLicenseCheckForTesting();
  }

  void SetPluginVmImagePref(std::string url, std::string hash) {
    ScopedDictPrefUpdate update(browser()->profile()->GetPrefs(),
                                plugin_vm::prefs::kPluginVmImage);
    base::Value::Dict& plugin_vm_image = update.Get();
    plugin_vm_image.Set("url", url);
    plugin_vm_image.Set("hash", hash);
  }

  void WaitForSetupToFinish() {
    base::RunLoop run_loop;
    view_->SetFinishedCallbackForTesting(
        base::BindOnce(&PluginVmInstallerViewBrowserTest::OnSetupFinished,
                       run_loop.QuitClosure()));

    run_loop.Run();
    content::RunAllTasksUntilIdle();
  }

  void CheckSetupFailed() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::mojom::DialogButton::kOk),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_RETRY_BUTTON));
    EXPECT_EQ(view_->GetTitle(),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_ERROR_TITLE));
  }

  void CheckSetupIsFinishedSuccessfully() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::mojom::DialogButton::kCancel),
              l10n_util::GetStringUTF16(IDS_APP_CLOSE));
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::mojom::DialogButton::kOk),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_LAUNCH_BUTTON));
    EXPECT_EQ(view_->GetTitle(), l10n_util::GetStringUTF16(
                                     IDS_PLUGIN_VM_INSTALLER_FINISHED_TITLE));
  }

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;

  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  raw_ptr<PluginVmInstallerView, DanglingUntriaged> view_;
  raw_ptr<ash::FakeConciergeClient, DanglingUntriaged> fake_concierge_client_;
  raw_ptr<ash::FakeVmPluginDispatcherClient, DanglingUntriaged>
      fake_vm_plugin_dispatcher_client_;

 private:
  void EnterpriseEnrollDevice() {
    scoped_stub_install_attributes_.Get()->SetCloudManaged("example.com",
                                                           "device_id");
  }

  void SetPluginVmPolicies() {
    // User polcies.
    browser()->profile()->GetPrefs()->SetBoolean(
        plugin_vm::prefs::kPluginVmAllowed, true);
    // Device policies.
    scoped_testing_cros_settings_.device_settings()->Set(ash::kPluginVmAllowed,
                                                         base::Value(true));
  }

  static void OnSetupFinished(base::OnceClosure quit_closure, bool success) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(quit_closure));
  }
};

class PluginVmInstallerViewBrowserTestWithFeatureEnabled
    : public PluginVmInstallerViewBrowserTest {
 public:
  PluginVmInstallerViewBrowserTestWithFeatureEnabled() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPluginVm, {}},
         {download::kDownloadServiceFeature, {{"start_up_delay_ms", "0"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test the dialog is actually can be launched.
IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       SetupShouldFinishSuccessfully) {
  AllowPluginVm();
  plugin_vm::SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();
  WaitForSetupToFinish();

  CheckSetupIsFinishedSuccessfully();
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       SetupShouldFireAccessibilityEvents) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());

  AllowPluginVm();
  plugin_vm::SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);
  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  auto* title_view = view_->GetTitleViewForTesting();
  EXPECT_NE(nullptr, title_view);

  auto* message_view = view_->GetMessageViewForTesting();
  EXPECT_NE(nullptr, message_view);

  auto* progress_view = view_->GetDownloadProgressMessageViewForTesting();
  EXPECT_NE(nullptr, progress_view);

  // Views should only fire property-change events when the property changes;
  // not when a value is initialized. As a result, there should not be any
  // text-changed accessibility fired as a result of the introductory/set-up
  // text being displayed.
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, title_view));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, message_view));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, progress_view));

  counter.ResetAllCounts();
  view_->AcceptDialog();

  // Once the installation has been accepted, the message and title labels are
  // changed to indicate the installation has begun. Each label should have
  // fired an accessibility event for this change. Because the download has not
  // started, there should be no event from the download progress label.
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged, title_view));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged, message_view));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, progress_view));

  counter.ResetAllCounts();
  WaitForSetupToFinish();

  // During the installation process, the title remains the same until the
  // installation is complete. There should be an accessibility event for the
  // title changing to the setup-complete text.
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged, title_view));

  // During the installation process, the message changes three times:
  // downloading, configuring, ready to use. Each time there should be an
  // accessibility event for the change.
  EXPECT_EQ(3, counter.GetCount(ax::mojom::Event::kTextChanged, message_view));

  // During the download process, there are periodic updates showing the
  // amount downloaded thus far. There are six such updates in this test:
  // the first "0 GB" and the next five "0.0 GB". Each time the text changes,
  // there should be an accessibility event for the change. Since the text
  // changed twice, there should be two updates.
  EXPECT_EQ(2, counter.GetCount(ax::mojom::Event::kTextChanged, progress_view));

  CheckSetupIsFinishedSuccessfully();
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       SetupShouldFailAsHashesDoNotMatch) {
  AllowPluginVm();
  // Reset PluginVmImage hash to non-matching.
  SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                       kNonMatchingHash);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();
  WaitForSetupToFinish();

  CheckSetupFailed();
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       SetupShouldFailAsImportingFails) {
  AllowPluginVm();
  SetPluginVmImagePref(embedded_test_server()->GetURL(kJpgFile).spec(),
                       kJpgFileHash);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();
  WaitForSetupToFinish();

  CheckSetupFailed();
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       CouldRetryAfterFailedSetup) {
  AllowPluginVm();
  // Reset PluginVmImage hash to non-matching.
  SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                       kNonMatchingHash);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();
  WaitForSetupToFinish();

  CheckSetupFailed();

  plugin_vm::SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);
  SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                       kZipFileHash);

  // Retry button clicked to retry the download.
  view_->AcceptDialog();

  WaitForSetupToFinish();

  CheckSetupIsFinishedSuccessfully();
}

IN_PROC_BROWSER_TEST_F(
    PluginVmInstallerViewBrowserTest,
    SetupShouldShowDisallowedMessageIfPluginVmIsNotAllowedToRun) {
  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();

  std::u16string app_name = l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME);
  EXPECT_FALSE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());
  EXPECT_EQ(view_->GetTitle(),
            l10n_util::GetStringFUTF16(
                IDS_PLUGIN_VM_INSTALLER_NOT_ALLOWED_TITLE, app_name));
  EXPECT_EQ(
      view_->GetMessage(),
      l10n_util::GetStringFUTF16(
          IDS_PLUGIN_VM_INSTALLER_NOT_ALLOWED_MESSAGE, app_name,
          base::NumberToString16(
              static_cast<std::underlying_type_t<
                  plugin_vm::PluginVmInstaller::FailureReason>>(
                  plugin_vm::PluginVmInstaller::FailureReason::NOT_ALLOWED))));
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       SetupShouldLaunchIfImageAlreadyImported) {
  AllowPluginVm();

  // Setup concierge and the dispatcher for VM already imported.
  vm_tools::concierge::ListVmDisksResponse list_vm_disks_response;
  list_vm_disks_response.set_success(true);
  auto* image = list_vm_disks_response.add_images();
  image->set_name(plugin_vm::kPluginVmName);
  image->set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_PLUGINVM);
  fake_concierge_client_->set_list_vm_disks_response(list_vm_disks_response);

  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  fake_vm_plugin_dispatcher_client_->set_list_vms_response(list_vms_response);

  fake_vm_plugin_dispatcher_client_->set_start_vm_response(
      vm_tools::plugin_dispatcher::StartVmResponse());

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();
  WaitForSetupToFinish();

  // Installer should be closed.
  EXPECT_EQ(nullptr, PluginVmInstallerView::GetActiveViewForTesting());
}
