// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views_test.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/interaction/expect_call_in_scope.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_getter.h"
#endif

OmniboxPopupViewViewsTest::ThemeChangeWaiter::~ThemeChangeWaiter() {
  waiter_.WaitForThemeChanged();
  // Theme changes propagate asynchronously in DesktopWindowTreeHostX11::
  // FrameTypeChanged(), so ensure all tasks are consumed.
  content::RunAllPendingInMessageLoop();
}

views::Widget* OmniboxPopupViewViewsTest::CreatePopupForTestQuery() {
  const auto* autocomplete_controller = controller()->autocomplete_controller();
  EXPECT_TRUE(autocomplete_controller->result().empty());
  EXPECT_FALSE(popup_view()->IsOpen());
  EXPECT_FALSE(GetPopupWidget());

  // Verify that the on-shown callback is called at the correct time.
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, popup_callback);
  const auto subscription = popup_view()->AddOpenListener(popup_callback.Get());

  EXPECT_CALL_IN_SCOPE(popup_callback, Run, {
    edit_model()->SetUserText(u"foo");
    AutocompleteInput input(
        u"foo", metrics::OmniboxEventProto::BLANK,
        ChromeAutocompleteSchemeClassifier(browser()->profile()));
    input.set_omit_asynchronous_matches(true);
    controller()->StartAutocomplete(input);

    EXPECT_FALSE(autocomplete_controller->result().empty());
    EXPECT_TRUE(popup_view()->IsOpen());
  });

  views::Widget* popup = GetPopupWidget();
  EXPECT_TRUE(popup);
  return popup;
}

void OmniboxPopupViewViewsTest::UseDefaultTheme() {
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

void OmniboxPopupSuggestionGroupHeadersTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");

  embedded_test_server()->ServeFilesFromDirectory(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA));

  // Register custom request handler for SRP URL.
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        // Check if the request is for our search path
        if (request.relative_url.starts_with("/search?")) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);  // Return 200 OK
          response->set_content("<html><body>Mock SRP Content</body></html>");
          response->set_content_type("text/html");
          return response;
        }
        // Let other handlers deal with other requests (like /empty.html)
        return nullptr;
      }));

  ASSERT_TRUE(embedded_test_server()->Start());
}

void OmniboxPopupSuggestionGroupHeadersTest::TearDownOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}
