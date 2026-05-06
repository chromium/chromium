// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_MOJO_VISIBILITY_GUARD_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_MOJO_VISIBILITY_GUARD_H_

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "components/version_info/version_info.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/mojo_buildflags.h"

// Command line switch to disable crashes on Mojo visibility violations to make
// disabling the crash possible without rebuilding the browser.
inline constexpr char kDisableCrashOnOmniboxPopupMojoVisibilitySwitch[] =
    "disable-crash-on-omnibox-popup-mojo-visibility";

// A Mojo MessageFilter to help ensure that Mojo calls are only dispatched when
// a visibility predicate is met. If the predicate is false, it logs an error
// and crashes in non-official builds.
template <typename Interface, typename MethodType = uint32_t>
class MojoVisibilityFilter : public mojo::MessageFilter {
 public:
  explicit MojoVisibilityFilter(
      base::RepeatingCallback<bool()> is_visible_callback,
      std::vector<MethodType> allowlisted_methods = {})
      : is_visible_callback_(std::move(is_visible_callback)),
        allowlisted_methods_(std::move(allowlisted_methods)) {}

  ~MojoVisibilityFilter() override = default;

  // mojo::MessageFilter:
  bool WillDispatch(mojo::Message* message) override {
    if (!is_visible_callback_.Run()) {
      uint32_t name = message->name();
      if (std::find(allowlisted_methods_.begin(), allowlisted_methods_.end(),
                    static_cast<MethodType>(name)) !=
          allowlisted_methods_.end()) {
        return true;
      }

      const char* method_name = Interface::MessageToMethodName_(*message);

      bool should_crash = !version_info::IsOfficialBuild() &&
                          !base::CommandLine::ForCurrentProcess()->HasSwitch(
                              kDisableCrashOnOmniboxPopupMojoVisibilitySwitch);

      logging::LogSeverity severity =
          should_crash ? logging::LOGGING_FATAL : logging::LOGGING_ERROR;
      logging::LogMessage(__FILE__, __LINE__, severity).stream()
          << "Mojo call " << Interface::Name_
          << "::" << (method_name ? method_name : "unknown")
          << " (name=" << name
          << ") was made while the Omnibox popup was not shown. "
#if !BUILDFLAG(MOJO_TRACE_ENABLED)
          << "Hint: set enable_mojo_tracing=true in gn args to see the "
             "method name in the logs."
#endif
          ;
    }
    return true;
  }

  void DidDispatchOrReject(mojo::Message* message, bool accepted) override {}

 private:
  base::RepeatingCallback<bool()> is_visible_callback_;
  std::vector<MethodType> allowlisted_methods_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_MOJO_VISIBILITY_GUARD_H_
