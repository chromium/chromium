// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dark_mode_manager_linux.h"

#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/linux/fake_linux_ui.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

namespace {

class MockLinuxUi : public FakeLinuxUi {
 public:
  MOCK_METHOD(ui::NativeTheme*, GetNativeTheme, (), (const override));
  MOCK_METHOD(void, SetDarkTheme, (bool dark), (override));
};

class MockNativeTheme : public NativeTheme {
 public:
  MockNativeTheme() : NativeTheme(false) {}
  ~MockNativeTheme() override = default;

  void SetUseDarkColors(bool use_dark_colors) {
    set_use_dark_colors(use_dark_colors);
    set_preferred_color_scheme(use_dark_colors
                                   ? NativeTheme::PreferredColorScheme::kDark
                                   : NativeTheme::PreferredColorScheme::kLight);
    NotifyOnNativeThemeUpdated();
  }

  // Mock some pure-virtual methods even though they're not used.
  MOCK_METHOD(gfx::Size,
              GetPartSize,
              (Part part, State state, const ExtraParams& extra),
              (const override));
  MOCK_METHOD(void,
              Paint,
              (cc::PaintCanvas * canvas,
               const ui::ColorProvider* color_provider,
               Part part,
               State state,
               const gfx::Rect& rect,
               const ExtraParams& extra,
               ColorScheme color_scheme,
               const absl::optional<SkColor>& accent_color),
              (const override));
  MOCK_METHOD(bool, SupportsNinePatch, (Part part), (const override));
  MOCK_METHOD(gfx::Size, GetNinePatchCanvasSize, (Part part), (const override));
  MOCK_METHOD(gfx::Rect, GetNinePatchAperture, (Part part), (const override));
};

ACTION_P2(RegisterSignalCallback, signal_callback, connected_callback) {
  *signal_callback = arg2;
  *connected_callback = std::move(*arg3);
}

ACTION_P(MethodCallback, response_callback, error_callback) {
  *response_callback = std::move(*arg2);
  *error_callback = std::move(*arg3);
}

// Matches a method call to the specified dbus target.
MATCHER_P2(Calls, interface, member, "") {
  return arg->GetInterface() == interface && arg->GetMember() == member;
}

}  // namespace

using testing::_;
using testing::ByMove;
using testing::Return;
using testing::StrictMock;

class DarkModeManagerLinuxTest : public testing::Test {
 public:
  ~DarkModeManagerLinuxTest() override = default;

 protected:
  bool ManagerPrefersDarkTheme() const { return manager_->prefer_dark_theme(); }

  dbus::ObjectProxy::SignalCallback& setting_changed_callback() {
    return setting_changed_callback_;
  }
  dbus::ObjectProxy::OnConnectedCallback& signal_connected_callback() {
    return signal_connected_callback_;
  }
  dbus::MockObjectProxy::ResponseCallback& response_callback() {
    return response_callback_;
  }
  dbus::MockObjectProxy::ErrorCallback& error_callback() {
    return error_callback_;
  }
  MockNativeTheme* mock_native_theme() { return mock_native_theme_.get(); }
  MockLinuxUi* mock_linux_ui() { return mock_linux_ui_.get(); }

 private:
  void SetUp() override {
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_portal_proxy_ =
        base::MakeRefCounted<StrictMock<dbus::MockObjectProxy>>(
            mock_bus_.get(), DarkModeManagerLinux::kFreedesktopSettingsService,
            dbus::ObjectPath(
                DarkModeManagerLinux::kFreedesktopSettingsObjectPath));

    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(
                    DarkModeManagerLinux::kFreedesktopSettingsService,
                    dbus::ObjectPath(
                        DarkModeManagerLinux::kFreedesktopSettingsObjectPath)))
        .WillOnce(Return(mock_portal_proxy_.get()));

    EXPECT_CALL(
        *mock_portal_proxy_,
        DoConnectToSignal(DarkModeManagerLinux::kFreedesktopSettingsInterface,
                          DarkModeManagerLinux::kSettingChangedSignal, _, _))
        .WillOnce(RegisterSignalCallback(&setting_changed_callback_,
                                         &signal_connected_callback_));

    EXPECT_CALL(*mock_portal_proxy_,
                DoCallMethodWithErrorCallback(
                    Calls(DarkModeManagerLinux::kFreedesktopSettingsInterface,
                          DarkModeManagerLinux::kReadMethod),
                    _, _, _))
        .WillOnce(MethodCallback(&response_callback_, &error_callback_));

    mock_linux_ui_ = std::make_unique<MockLinuxUi>();
    linux_ui_themes_ = std::vector<LinuxUiTheme*>{mock_linux_ui_.get()};

    mock_native_theme_ = std::make_unique<MockNativeTheme>();
    EXPECT_CALL(*mock_linux_ui_, GetNativeTheme())
        .WillOnce(Return(mock_native_theme_.get()));

    manager_ = std::make_unique<DarkModeManagerLinux>(
        mock_bus_, mock_linux_ui_.get(), &linux_ui_themes_,
        std::vector<NativeTheme*>{mock_native_theme_.get()});

    EXPECT_FALSE(manager_->prefer_dark_theme());
    EXPECT_FALSE(mock_native_theme_->ShouldUseDarkColors());
    EXPECT_EQ(mock_native_theme_->GetPreferredColorScheme(),
              NativeTheme::PreferredColorScheme::kLight);
  }

  void TearDown() override {
    EXPECT_CALL(*mock_bus_, GetDBusTaskRunner()).WillOnce(Return(nullptr));
    manager_.reset();
  }

  std::unique_ptr<MockLinuxUi> mock_linux_ui_;
  std::vector<LinuxUiTheme*> linux_ui_themes_;

  std::unique_ptr<MockNativeTheme> mock_native_theme_;

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_portal_proxy_;

  dbus::ObjectProxy::SignalCallback setting_changed_callback_;
  dbus::ObjectProxy::OnConnectedCallback signal_connected_callback_;

  dbus::MockObjectProxy::ResponseCallback response_callback_;
  dbus::MockObjectProxy::ErrorCallback error_callback_;

  std::unique_ptr<DarkModeManagerLinux> manager_;
};

TEST_F(DarkModeManagerLinuxTest, UseNativeThemeSetting) {
  // Set the native theme preference before the async DBus calls complete.
  mock_native_theme()->SetUseDarkColors(true);
  EXPECT_TRUE(ManagerPrefersDarkTheme());
  mock_native_theme()->SetUseDarkColors(false);
  EXPECT_FALSE(ManagerPrefersDarkTheme());

  // Let the manager know the DBus method call and signal connection failed.
  dbus::MethodCall method_call(
      DarkModeManagerLinux::kFreedesktopSettingsInterface,
      DarkModeManagerLinux::kReadMethod);
  method_call.SetSerial(123);
  auto error = dbus::ErrorResponse::FromMethodCall(
      &method_call, "org.freedesktop.DBus.Error.Failed", "");
  std::move(error_callback()).Run(error.get());
  std::move(signal_connected_callback())
      .Run(DarkModeManagerLinux::kFreedesktopSettingsInterface,
           DarkModeManagerLinux::kSettingChangedSignal, false);

  // The native theme preference should still toggle the manager preference.
  mock_native_theme()->SetUseDarkColors(true);
  EXPECT_TRUE(ManagerPrefersDarkTheme());
  mock_native_theme()->SetUseDarkColors(false);
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
  variant_writer.AppendVariantOfUint32(
      DarkModeManagerLinux::kFreedesktopColorSchemeDark);
  writer.CloseContainer(&variant_writer);
  EXPECT_CALL(*mock_linux_ui(), SetDarkTheme(true));
  std::move(response_callback()).Run(response.get());
  EXPECT_TRUE(ManagerPrefersDarkTheme());
  EXPECT_TRUE(mock_native_theme()->ShouldUseDarkColors());
  EXPECT_EQ(mock_native_theme()->GetPreferredColorScheme(),
            NativeTheme::PreferredColorScheme::kDark);

  // Changes in the portal preference should be processed by the manager and the
  // native theme should be updated.
  dbus::Signal signal(DarkModeManagerLinux::kFreedesktopSettingsInterface,
                      DarkModeManagerLinux::kSettingChangedSignal);
  dbus::MessageWriter signal_writer(&signal);
  signal_writer.AppendString(DarkModeManagerLinux::kSettingsNamespace);
  signal_writer.AppendString(DarkModeManagerLinux::kColorSchemeKey);
  signal_writer.AppendVariantOfUint32(0);
  EXPECT_CALL(*mock_linux_ui(), SetDarkTheme(false));
  std::move(setting_changed_callback()).Run(&signal);
  EXPECT_FALSE(ManagerPrefersDarkTheme());
  EXPECT_FALSE(mock_native_theme()->ShouldUseDarkColors());
  EXPECT_EQ(mock_native_theme()->GetPreferredColorScheme(),
            NativeTheme::PreferredColorScheme::kLight);

  // The native theme preference should have no effect when the portal
  // preference is being used.
  mock_native_theme()->SetUseDarkColors(true);
  EXPECT_FALSE(ManagerPrefersDarkTheme());
  mock_native_theme()->SetUseDarkColors(false);
  EXPECT_FALSE(ManagerPrefersDarkTheme());
}

}  // namespace ui
