// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dark_mode_manager_linux.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/dbus/xdg/portal.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/linux/fake_linux_ui.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

namespace {

class MockLinuxUi : public FakeLinuxUi {
 public:
  MOCK_METHOD(ui::NativeTheme*, GetNativeTheme, (), (const override));
  MOCK_METHOD(void, SetDarkTheme, (bool dark), (override));
  MOCK_METHOD(void, SetAccentColor, (std::optional<SkColor> color), (override));
};

// Matches a method call to the specified dbus target.
MATCHER_P2(Calls, interface, member, "") {
  return arg->GetInterface() == interface && arg->GetMember() == member;
}

}  // namespace

using testing::_;
using testing::AtLeast;
using testing::ByMove;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

class DarkModeManagerLinuxTest : public testing::Test {
 public:
  ~DarkModeManagerLinuxTest() override = default;

 protected:
  bool ManagerPrefersDarkTheme() const {
    return manager_->preferred_color_scheme_ ==
           NativeTheme::PreferredColorScheme::kDark;
  }

  dbus::ObjectProxy::SignalCallback& setting_changed_callback() {
    return setting_changed_callback_;
  }
  dbus::ObjectProxy::OnConnectedCallback& signal_connected_callback() {
    return signal_connected_callback_;
  }
  dbus::ObjectProxy::ResponseOrErrorCallback& color_scheme_callback() {
    return color_scheme_callback_;
  }
  dbus::ObjectProxy::ResponseOrErrorCallback& accent_color_callback() {
    return accent_color_callback_;
  }
  MockOsSettingsProvider& os_settings_provider() {
    return os_settings_provider_;
  }
  MockLinuxUi* mock_linux_ui() { return mock_linux_ui_.get(); }

 private:
  void SetUp() override {
    dbus_xdg::SetPortalStateForTesting(
        dbus_xdg::PortalRegistrarState::kSuccess);
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_dbus_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
    mock_portal_proxy_ =
        base::MakeRefCounted<StrictMock<dbus::MockObjectProxy>>(
            mock_bus_.get(), DarkModeManagerLinux::kFreedesktopSettingsService,
            dbus::ObjectPath(
                DarkModeManagerLinux::kFreedesktopSettingsObjectPath));

    EXPECT_CALL(*mock_bus_, GetObjectProxy(DBUS_SERVICE_DBUS,
                                           dbus::ObjectPath(DBUS_PATH_DBUS)))
        .WillRepeatedly(Return(mock_dbus_proxy_.get()));

    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(
                    DarkModeManagerLinux::kFreedesktopSettingsService,
                    dbus::ObjectPath(
                        DarkModeManagerLinux::kFreedesktopSettingsObjectPath)))
        .WillRepeatedly(Return(mock_portal_proxy_.get()));

    EXPECT_CALL(
        *mock_portal_proxy_,
        ConnectToSignal(DarkModeManagerLinux::kFreedesktopSettingsInterface,
                        DarkModeManagerLinux::kSettingChangedSignal, _, _))
        .WillOnce(
            [&](const std::string& interface_name,
                const std::string& signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
              setting_changed_callback_ = signal_callback;
              signal_connected_callback_ = std::move(on_connected_callback);
            });

    EXPECT_CALL(*mock_portal_proxy_,
                CallMethodWithErrorResponse(
                    Calls(DarkModeManagerLinux::kFreedesktopSettingsInterface,
                          DarkModeManagerLinux::kReadMethod),
                    _, _))
        .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                      dbus::ObjectProxy::ResponseOrErrorCallback callback) {
          color_scheme_callback_ = std::move(callback);
        })
        .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                      dbus::ObjectProxy::ResponseOrErrorCallback callback) {
          accent_color_callback_ = std::move(callback);
        });

    mock_linux_ui_ = std::make_unique<MockLinuxUi>();
    linux_ui_themes_ = std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>{
        mock_linux_ui_.get()};
    auto* const native_theme = ui::NativeTheme::GetInstanceForNativeUi();
    native_theme->set_preferred_color_scheme(
        NativeTheme::PreferredColorScheme::kNoPreference);
    EXPECT_CALL(*mock_linux_ui_, GetNativeTheme())
        .WillOnce(Return(native_theme));

    enable_portal_accent_color_.InitAndEnableFeature(
        features::kUsePortalAccentColor);

    manager_ = std::make_unique<DarkModeManagerLinux>(
        mock_bus_, mock_linux_ui_.get(), &linux_ui_themes_);

    EXPECT_FALSE(ManagerPrefersDarkTheme());
    EXPECT_EQ(native_theme->preferred_color_scheme(),
              NativeTheme::PreferredColorScheme::kNoPreference);
    EXPECT_FALSE(native_theme->user_color().has_value());
  }

  void TearDown() override {
    manager_.reset();
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
  }

  std::unique_ptr<MockLinuxUi> mock_linux_ui_;
  std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>> linux_ui_themes_;

  MockOsSettingsProvider os_settings_provider_;

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_dbus_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_portal_proxy_;

  dbus::ObjectProxy::SignalCallback setting_changed_callback_;
  dbus::ObjectProxy::OnConnectedCallback signal_connected_callback_;

  dbus::ObjectProxy::ResponseOrErrorCallback color_scheme_callback_;
  dbus::ObjectProxy::ResponseOrErrorCallback accent_color_callback_;

  base::test::ScopedFeatureList enable_portal_accent_color_;

  std::unique_ptr<DarkModeManagerLinux> manager_;
};

TEST_F(DarkModeManagerLinuxTest, UseNativeThemeSetting) {
  // Set the native theme preference before the async DBus calls complete.
  os_settings_provider().SetPreferredColorScheme(
      NativeTheme::PreferredColorScheme::kDark);
  EXPECT_TRUE(ManagerPrefersDarkTheme());
  os_settings_provider().SetPreferredColorScheme(
      NativeTheme::PreferredColorScheme::kLight);
  EXPECT_FALSE(ManagerPrefersDarkTheme());

  // Let the manager know the DBus method call and signal connection failed.
  dbus::MethodCall method_call(
      DarkModeManagerLinux::kFreedesktopSettingsInterface,
      DarkModeManagerLinux::kReadMethod);
  method_call.SetSerial(123);
  auto error = dbus::ErrorResponse::FromMethodCall(
      &method_call, "org.freedesktop.DBus.Error.Failed", "");
  std::move(color_scheme_callback()).Run(nullptr, error.get());
  std::move(signal_connected_callback())
      .Run(DarkModeManagerLinux::kFreedesktopSettingsInterface,
           DarkModeManagerLinux::kSettingChangedSignal, false);

  // The native theme preference should still toggle the manager preference.
  os_settings_provider().SetPreferredColorScheme(
      NativeTheme::PreferredColorScheme::kDark);
  EXPECT_TRUE(ManagerPrefersDarkTheme());
  os_settings_provider().SetPreferredColorScheme(
      NativeTheme::PreferredColorScheme::kLight);
  EXPECT_FALSE(ManagerPrefersDarkTheme());
}

TEST_F(DarkModeManagerLinuxTest, UsePortalSetting) {
  // Let the manager know the DBus method call and signal connection succeeded.
  dbus::MethodCall method_call(
      DarkModeManagerLinux::kFreedesktopSettingsInterface,
      DarkModeManagerLinux::kReadMethod);
  method_call.SetSerial(123);
  auto response = dbus::Response::FromMethodCall(&method_call);
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter variant_writer(nullptr);
  writer.OpenVariant("v", &variant_writer);
  variant_writer.AppendVariantOfUint32(static_cast<uint32_t>(
      DarkModeManagerLinux::FreedesktopColorScheme::kDark));
  writer.CloseContainer(&variant_writer);
  EXPECT_CALL(*mock_linux_ui(), SetDarkTheme(true));
  std::move(color_scheme_callback()).Run(response.get(), nullptr);
  EXPECT_TRUE(ManagerPrefersDarkTheme());
  auto* const native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  EXPECT_EQ(native_theme->preferred_color_scheme(),
            NativeTheme::PreferredColorScheme::kDark);

  // Changes in the portal preference should be processed by the manager and the
  // native theme should be updated.
  dbus::Signal signal(DarkModeManagerLinux::kFreedesktopSettingsInterface,
                      DarkModeManagerLinux::kSettingChangedSignal);
  dbus::MessageWriter signal_writer(&signal);
  signal_writer.AppendString(DarkModeManagerLinux::kSettingsNamespace);
  signal_writer.AppendString(DarkModeManagerLinux::kColorSchemeKey);
  signal_writer.AppendVariantOfUint32(static_cast<uint32_t>(
      DarkModeManagerLinux::FreedesktopColorScheme::kLight));
  EXPECT_CALL(*mock_linux_ui(), SetDarkTheme(false));
  std::move(setting_changed_callback()).Run(&signal);
  EXPECT_FALSE(ManagerPrefersDarkTheme());
  EXPECT_EQ(native_theme->preferred_color_scheme(),
            NativeTheme::PreferredColorScheme::kLight);

  // The native theme preference should have no effect when the portal
  // preference is being used.
  os_settings_provider().SetPreferredColorScheme(
      NativeTheme::PreferredColorScheme::kDark);
  EXPECT_FALSE(ManagerPrefersDarkTheme());
  os_settings_provider().SetPreferredColorScheme(
      NativeTheme::PreferredColorScheme::kLight);
  EXPECT_FALSE(ManagerPrefersDarkTheme());
}

TEST_F(DarkModeManagerLinuxTest, UsePortalAccentColor) {
  // Let the manager know the DBus method call and signal connection succeeded.
  dbus::MethodCall method_call(
      DarkModeManagerLinux::kFreedesktopSettingsInterface,
      DarkModeManagerLinux::kReadMethod);
  method_call.SetSerial(123);
  auto response = dbus::Response::FromMethodCall(&method_call);
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter outer_variant_writer(nullptr);
  writer.OpenVariant("v", &outer_variant_writer);
  dbus::MessageWriter inner_variant_writer(nullptr);
  outer_variant_writer.OpenVariant("(ddd)", &inner_variant_writer);
  dbus::MessageWriter struct1_writer(nullptr);
  inner_variant_writer.OpenStruct(&struct1_writer);
  struct1_writer.AppendDouble(0.0);
  struct1_writer.AppendDouble(0.5);
  struct1_writer.AppendDouble(1.0);
  inner_variant_writer.CloseContainer(&struct1_writer);
  outer_variant_writer.CloseContainer(&inner_variant_writer);
  writer.CloseContainer(&outer_variant_writer);
  static constexpr std::optional<SkColor> kExpectedColor1 =
      SkColorSetRGB(0, 127, 255);
  EXPECT_CALL(*mock_linux_ui(), SetAccentColor(kExpectedColor1));
  std::move(accent_color_callback()).Run(response.get(), nullptr);
  auto* const native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  EXPECT_EQ(native_theme->user_color(), kExpectedColor1);
  Mock::VerifyAndClearExpectations(mock_linux_ui());

  // Changes in the portal accent color should be processed by the manager and
  // the native theme should be updated.
  dbus::Signal signal(DarkModeManagerLinux::kFreedesktopSettingsInterface,
                      DarkModeManagerLinux::kSettingChangedSignal);
  dbus::MessageWriter signal_writer(&signal);
  signal_writer.AppendString(DarkModeManagerLinux::kSettingsNamespace);
  signal_writer.AppendString(DarkModeManagerLinux::kAccentColorKey);
  dbus::MessageWriter variant_writer(nullptr);
  signal_writer.OpenVariant("(ddd)", &variant_writer);
  dbus::MessageWriter struct2_writer(nullptr);
  variant_writer.OpenStruct(&struct2_writer);
  struct2_writer.AppendDouble(1.0);
  struct2_writer.AppendDouble(0.5);
  struct2_writer.AppendDouble(0.0);
  variant_writer.CloseContainer(&struct2_writer);
  signal_writer.CloseContainer(&variant_writer);
  static constexpr std::optional<SkColor> kExpectedColor2 =
      SkColorSetRGB(255, 127, 0);
  EXPECT_CALL(*mock_linux_ui(), SetAccentColor(kExpectedColor2));
  std::move(setting_changed_callback()).Run(&signal);
  EXPECT_EQ(native_theme->user_color(), kExpectedColor2);
}

}  // namespace ui
