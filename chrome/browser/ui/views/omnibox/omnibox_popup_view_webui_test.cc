// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui_test.h"

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "ui/base/interaction/expect_call_in_scope.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_getter.h"
#endif

OmniboxPopupViewWebUITest::OmniboxPopupViewWebUITest() = default;
OmniboxPopupViewWebUITest::~OmniboxPopupViewWebUITest() = default;

OmniboxPopupViewWebUITest::ThemeChangeWaiter::~ThemeChangeWaiter() {
  waiter_.WaitForThemeChanged();
  // Theme changes propagate asynchronously in DesktopWindowTreeHostX11::
  // FrameTypeChanged(), so ensure all tasks are consumed.
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).Run();
}

void OmniboxPopupViewWebUITest::CreatePopupForTestQuery() {
  auto* autocomplete_controller = controller()->autocomplete_controller();
  EXPECT_TRUE(autocomplete_controller->result().empty());
  EXPECT_FALSE(popup_view()->IsOpen());

  // Verify that the on-shown callback is called at the correct time.
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, popup_callback);
  const auto subscription = popup_view()->AddOpenListener(popup_callback.Get());

  EXPECT_CALL_IN_SCOPE(popup_callback, Run, {
    edit_model()->SetUserText(u"foo");
    AutocompleteInput input(
        u"foo", metrics::OmniboxEventProto::BLANK,
        ChromeAutocompleteSchemeClassifier(browser()->profile()));
    input.set_omit_asynchronous_matches(true);
    autocomplete_controller->Start(input);

    EXPECT_FALSE(autocomplete_controller->result().empty());
    EXPECT_TRUE(popup_view()->IsOpen());
  });
}

void OmniboxPopupViewWebUITest::UseDefaultTheme() {
#if BUILDFLAG(IS_LINUX)
  // Normally it would be sufficient to call ThemeService::UseDefaultTheme()
  // which sets the kUsesSystemTheme user pref on the browser's profile.
  // However BrowserThemeProvider::GetColorProviderColor() currently does not
  // pass an aura::Window to LinuxUI::GetNativeTheme() - which means that the
  // NativeThemeGtk instance will always be returned.
  // TODO(crbug.com/40217733): Remove this once GTK passthrough is fully
  // supported.
  ui::LinuxUiGetter::set_instance(nullptr);
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();

  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  if (!theme_service->UsingDefaultTheme()) {
    ThemeChangeWaiter wait(theme_service);
    theme_service->UseDefaultTheme();
  }
  ASSERT_TRUE(theme_service->UsingDefaultTheme());
#endif  // BUILDFLAG(IS_LINUX)
}

void OmniboxPopupViewWebUITest::SetUp() {
  feature_list_.InitAndEnableFeature(omnibox::kWebUIOmniboxPopup);
  InProcessBrowserTest::SetUp();
}

void OmniboxPopupViewWebUITest::WaitForHandler() {
  if (!popup_view()->presenter_->IsHandlerReady()) {
    base::RunLoop loop;
    auto quit = loop.QuitClosure();
    auto runner = base::ThreadPool::CreateTaskRunner(base::TaskTraits());
    runner->PostTask(FROM_HERE,
                     base::BindOnce(&OmniboxPopupViewWebUITest::WaitInternal,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    popup_view()->presenter_.get(), &quit));
    loop.Run();
    CHECK(popup_view()->presenter_->IsHandlerReady());
  }
}

void OmniboxPopupViewWebUITest::WaitInternal(OmniboxPopupPresenter* presenter,
                                             base::RepeatingClosure* closure) {
  while (!presenter->IsHandlerReady()) {
    base::PlatformThread::Sleep(base::Milliseconds(1));
  }
  closure->Run();
}
