// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/arc/arc_app_dialog.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/arc_usb_host_permission_manager.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace arc {

class ArcAppUninstallDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  ArcAppUninstallDialogViewBrowserTest() {}

  ArcAppUninstallDialogViewBrowserTest(
      const ArcAppUninstallDialogViewBrowserTest&) = delete;
  ArcAppUninstallDialogViewBrowserTest& operator=(
      const ArcAppUninstallDialogViewBrowserTest&) = delete;

  ~ArcAppUninstallDialogViewBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    profile_ = browser()->profile();

    arc::SetArcPlayStoreEnabledForProfile(profile_, true);

    arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
    DCHECK(arc_app_list_pref_);

    base::RunLoop run_loop;
    arc_app_list_pref_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
    arc_app_list_pref_->app_connection_holder()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_app_list_pref_->app_connection_holder());

    // In this setup, we have one app and one shortcut which share one package.
    std::vector<mojom::AppInfoPtr> one_app;
    one_app.emplace_back(mojom::AppInfo::New("Fake App 0", "fake.package.0",
                                             "fake.app.0.activity",
                                             false /* sticky */));

    app_instance_->SendRefreshAppList(one_app);

    mojom::ShortcutInfo shortcut;
    shortcut.name = "Fake Shortcut 0";
    shortcut.package_name = "fake.package.0";
    shortcut.intent_uri = "Fake Shortcut uri 0";
    app_instance_->SendInstallShortcut(shortcut);

    std::vector<mojom::ArcPackageInfoPtr> packages;
    packages.emplace_back(mojom::ArcPackageInfo::New(
        "fake.package.0" /* package_name */, 0 /* package_version */,
        0 /* last_backup_android_id */, 0 /* last_backup_time */,
        false /* sync */));
    app_instance_->SendRefreshPackageList(std::move(packages));
  }

  void TearDownOnMainThread() override {
    arc_app_list_pref_->app_connection_holder()->CloseInstance(
        app_instance_.get());
    app_instance_.reset();
    ArcSessionManager::Get()->Shutdown();
  }

  // Ensures the ArcAppDialogView is destoryed.
  void TearDown() override { ASSERT_FALSE(IsArcAppDialogViewAliveForTest()); }

  ArcAppListPrefs* arc_app_list_pref() { return arc_app_list_pref_; }

  FakeAppInstance* instance() { return app_instance_.get(); }

  Profile* profile() { return profile_; }

 private:
  raw_ptr<ArcAppListPrefs, DanglingUntriaged> arc_app_list_pref_ = nullptr;

  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;

  std::unique_ptr<arc::FakeAppInstance> app_instance_;
};

class ArcAppPermissionDialogViewBrowserTest
    : public ArcAppUninstallDialogViewBrowserTest {
 public:
  ArcAppPermissionDialogViewBrowserTest() {}

  ArcAppPermissionDialogViewBrowserTest(
      const ArcAppPermissionDialogViewBrowserTest&) = delete;
  ArcAppPermissionDialogViewBrowserTest& operator=(
      const ArcAppPermissionDialogViewBrowserTest&) = delete;

  // InProcessBrowserTest:
  ~ArcAppPermissionDialogViewBrowserTest() override = default;

  void InstallExtraPackage(int id) {
    mojom::AppInfoPtr app = mojom::AppInfo::New(
        base::StringPrintf("Fake App %d", id),
        base::StringPrintf("fake.package.%d", id),
        base::StringPrintf("fake.app.%d.activity", id), false);
    instance()->SendAppAdded(*app);

    instance()->SendPackageAdded(arc::mojom::ArcPackageInfo::New(
        base::StringPrintf("fake.package.%d", id) /* package_name */,
        id /* package_version */, id /* last_backup_android_id */,
        0 /* last_backup_time */, false /* sync */));

    // AppService uses mojom, so flush mojom calls to add the app to AppService.
    auto* app_service_proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile());
    ASSERT_TRUE(app_service_proxy);
  }

  void set_accepted(bool accepted) { accepted_ = accepted; }

  bool accepted() const { return accepted_; }

  base::WeakPtr<ArcAppPermissionDialogViewBrowserTest> weak_ptr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void RequestScanDeviceListPermission(
      ArcUsbHostPermissionManager* arc_usb_permission_manager,
      const std::string& package_name) {
    arc_usb_permission_manager->RequestUsbScanDeviceListPermission(
        package_name,
        base::BindOnce(&ArcAppPermissionDialogViewBrowserTest::set_accepted,
                       weak_ptr()));
  }

  void RequestAccessPermission(
      ArcUsbHostPermissionManager* arc_usb_permission_manager,
      const std::string& package_name) {
    arc_usb_permission_manager->RequestUsbAccessPermission(
        package_name, guid_, serial_number_, manufacturer_string_,
        product_string_, vendor_id_, product_id_,
        base::BindOnce(&ArcAppPermissionDialogViewBrowserTest::set_accepted,
                       weak_ptr()));
  }

  const std::string& guid() const { return guid_; }
  const std::u16string& serial_number() const { return serial_number_; }
  uint16_t vendor_id() const { return vendor_id_; }
  uint16_t product_id() const { return product_id_; }

 private:
  // boolean used to verify dialog result if |set_accepted| is passed as
  // callback. Used in USB basic permission flow test.
  bool accepted_ = false;

  // USB flow test related.
  const std::string guid_ = "TestGuidXXXXXX";
  const std::u16string serial_number_ = u"TestSerialNumber";
  const std::u16string manufacturer_string_ = u"Factory";
  const std::u16string product_string_ = u"Product";
  uint16_t vendor_id_ = 123;
  uint16_t product_id_ = 456;

  base::WeakPtrFactory<ArcAppPermissionDialogViewBrowserTest> weak_ptr_factory_{
      this};
};

// Basic flow of requesting scan device list or access permission.
IN_PROC_BROWSER_TEST_F(ArcAppPermissionDialogViewBrowserTest,
                       ArcUsbPermissionBasicFlow) {
  ArcUsbHostPermissionManager* arc_usb_permission_manager =
      ArcUsbHostPermissionManager::GetForBrowserContext(profile());
  DCHECK(arc_usb_permission_manager);

  // Invalid package name. Requesut is automatically rejected.
  const std::string invalid_package = "invalid_package";
  RequestScanDeviceListPermission(arc_usb_permission_manager, invalid_package);
  EXPECT_FALSE(IsArcAppDialogViewAliveForTest());
  EXPECT_FALSE(accepted());

  const std::string package_name = "fake.package.0";

  // Package sends scan devicelist request.
  RequestScanDeviceListPermission(arc_usb_permission_manager, package_name);

  // Dialog is shown. Call runs with false.
  EXPECT_TRUE(IsArcAppDialogViewAliveForTest());
  // Accept the dialog.
  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(true));
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsArcAppDialogViewAliveForTest());
  // Result will apply when next time the package tries to request the
  // permisson.
  EXPECT_FALSE(accepted());
  // Package tries to request scan device list permission again.
  RequestScanDeviceListPermission(arc_usb_permission_manager, package_name);
  EXPECT_FALSE(IsArcAppDialogViewAliveForTest());
  EXPECT_TRUE(accepted());

  set_accepted(false);

  // Package sends device access request.
  RequestAccessPermission(arc_usb_permission_manager, package_name);
  // Dialog is shown.
  EXPECT_TRUE(IsArcAppDialogViewAliveForTest());
  // Accept the dialog.
  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(true));
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsArcAppDialogViewAliveForTest());
  // Permission applies.
  EXPECT_TRUE(accepted());
  // Package sends same device access request again.
  RequestAccessPermission(arc_usb_permission_manager, package_name);
  // Dialog is not shown. Permission still applies.
  EXPECT_FALSE(IsArcAppDialogViewAliveForTest());
  EXPECT_TRUE(accepted());
}

// Multiple requests are sent at same time and are processed in request order.
// Previously accepted requests will be remembered.
IN_PROC_BROWSER_TEST_F(ArcAppPermissionDialogViewBrowserTest,
                       ArcUsbPermissionMultipleRequestFlow) {
  ArcUsbHostPermissionManager* arc_usb_permission_manager =
      ArcUsbHostPermissionManager::GetForBrowserContext(profile());
  DCHECK(arc_usb_permission_manager);

  InstallExtraPackage(1);
  InstallExtraPackage(2);

  const std::string package0 = "fake.package.0";
  const std::string package1 = "fake.package.1";
  const std::string package2 = "fake.package.2";

  // Package0 sends device access request.
  RequestAccessPermission(arc_usb_permission_manager, package0);

  // Package1 sends device access request.
  RequestAccessPermission(arc_usb_permission_manager, package1);

  // Package0 sends device access request again.
  RequestAccessPermission(arc_usb_permission_manager, package0);

  // Package2 sends device access request.
  RequestAccessPermission(arc_usb_permission_manager, package2);

  // Dialog is shown.
  EXPECT_TRUE(IsArcAppDialogViewAliveForTest());
  const auto& pending_requests =
      arc_usb_permission_manager->GetPendingRequestsForTesting();
  EXPECT_EQ(3u, pending_requests.size());
  EXPECT_EQ(package1, pending_requests[0].package_name());
  EXPECT_EQ(package0, pending_requests[1].package_name());
  EXPECT_EQ(package2, pending_requests[2].package_name());

  // Accept the dialog.
  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(true));
  content::RunAllPendingInMessageLoop();

  // Dialog is shown for the next request.
  EXPECT_TRUE(IsArcAppDialogViewAliveForTest());
  EXPECT_EQ(2u, pending_requests.size());
  EXPECT_EQ(package0, pending_requests[0].package_name());
  EXPECT_EQ(package2, pending_requests[1].package_name());

  // Accept the dialog.
  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(true));
  content::RunAllPendingInMessageLoop();

  // The 3rd request is the same to the first request so it's automatically
  // confirmed. Dialog is shown for the final request.
  EXPECT_TRUE(IsArcAppDialogViewAliveForTest());
  EXPECT_EQ(0u, pending_requests.size());

  // Reject the dialog.
  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(false));
  content::RunAllPendingInMessageLoop();

  // All requests are handled. No dialog is shown.
  EXPECT_FALSE(IsArcAppDialogViewAliveForTest());

  // Checks permissions.
  EXPECT_TRUE(arc_usb_permission_manager->HasUsbAccessPermission(
      package0, guid(), serial_number(), vendor_id(), product_id()));
  EXPECT_TRUE(arc_usb_permission_manager->HasUsbAccessPermission(
      package1, guid(), serial_number(), vendor_id(), product_id()));
  EXPECT_FALSE(arc_usb_permission_manager->HasUsbAccessPermission(
      package2, guid(), serial_number(), vendor_id(), product_id()));
}

// Package is removed when permission request is queued.
IN_PROC_BROWSER_TEST_F(ArcAppPermissionDialogViewBrowserTest,
                       ArcUsbPermissionPackageUninstallFlow) {
  ArcUsbHostPermissionManager* arc_usb_permission_manager =
      ArcUsbHostPermissionManager::GetForBrowserContext(profile());
  DCHECK(arc_usb_permission_manager);

  InstallExtraPackage(1);
  InstallExtraPackage(2);

  const std::string package0 = "fake.package.0";
  const std::string package1 = "fake.package.1";
  const std::string package2 = "fake.package.2";

  // Package0 sends device access request.
  RequestAccessPermission(arc_usb_permission_manager, package0);

  // Package1 sends device access request.
  RequestAccessPermission(arc_usb_permission_manager, package1);

  // Package2 sends device access request.
  RequestAccessPermission(arc_usb_permission_manager, package2);

  // Dialog is shown.
  EXPECT_TRUE(IsArcAppDialogViewAliveForTest());
  const auto& pending_requests =
      arc_usb_permission_manager->GetPendingRequestsForTesting();
  EXPECT_EQ(2u, pending_requests.size());
  EXPECT_EQ(package1, pending_requests[0].package_name());
  EXPECT_EQ(package2, pending_requests[1].package_name());

  // Uninstall package0 and package2.
  UninstallPackage(package0);
  UninstallPackage(package2);
  EXPECT_EQ(1u, pending_requests.size());
  EXPECT_EQ(package1, pending_requests[0].package_name());

  // Accept the dialog. But callback is ignored as package0 is uninstalled.
  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(true));
  content::RunAllPendingInMessageLoop();

  // Permision dialog for next request is shown.
  EXPECT_TRUE(IsArcAppDialogViewAliveForTest());
  EXPECT_EQ(0u, pending_requests.size());

  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(true));
  content::RunAllPendingInMessageLoop();

  EXPECT_FALSE(arc_usb_permission_manager->HasUsbAccessPermission(
      package0, guid(), serial_number(), vendor_id(), product_id()));
  EXPECT_TRUE(arc_usb_permission_manager->HasUsbAccessPermission(
      package1, guid(), serial_number(), vendor_id(), product_id()));
  EXPECT_FALSE(arc_usb_permission_manager->HasUsbAccessPermission(
      package2, guid(), serial_number(), vendor_id(), product_id()));
}

// Device is removed when permission request is queued.
IN_PROC_BROWSER_TEST_F(ArcAppPermissionDialogViewBrowserTest,
                       ArcUsbPermissionDeviceRemoveFlow) {
  ArcUsbHostPermissionManager* arc_usb_permission_manager =
      ArcUsbHostPermissionManager::GetForBrowserContext(profile());
  DCHECK(arc_usb_permission_manager);

  InstallExtraPackage(1);

  const std::string package0 = "fake.package.0";
  const std::string package1 = "fake.package.1";

  // Package0 sends device access request.
  RequestAccessPermission(arc_usb_permission_manager, package0);

  // Package1 sends device access request.
  RequestAccessPermission(arc_usb_permission_manager, package1);

  // Dialog is shown.
  EXPECT_TRUE(IsArcAppDialogViewAliveForTest());
  const auto& pending_requests =
      arc_usb_permission_manager->GetPendingRequestsForTesting();
  EXPECT_EQ(1u, pending_requests.size());
  EXPECT_EQ(package1, pending_requests[0].package_name());

  // Device is removed.
  arc_usb_permission_manager->DeviceRemoved(guid());
  EXPECT_EQ(0u, pending_requests.size());

  // Accept the dialog. But callback is ignored as device is removed.
  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(true));
  content::RunAllPendingInMessageLoop();

  EXPECT_FALSE(arc_usb_permission_manager->HasUsbAccessPermission(
      package0, guid(), serial_number(), vendor_id(), product_id()));
  EXPECT_FALSE(arc_usb_permission_manager->HasUsbAccessPermission(
      package1, guid(), serial_number(), vendor_id(), product_id()));
}

}  // namespace arc
