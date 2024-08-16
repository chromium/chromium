// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ACCESSIBILITY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ACCESSIBILITY_UI_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/webui_config.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"

namespace ui {
struct AXUpdatesAndEvents;
}

namespace content {
class ScopedAccessibilityMode;
class WebContents;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class AccessibilityUI;

class AccessibilityUIConfig
    : public content::DefaultWebUIConfig<AccessibilityUI> {
 public:
  AccessibilityUIConfig();
  ~AccessibilityUIConfig() override;
};

// Controls the accessibility web UI page.
class AccessibilityUI : public content::WebUIController {
 public:
  explicit AccessibilityUI(content::WebUI* web_ui);
  ~AccessibilityUI() override;
};

// Observes accessibility events from web contents.
class AccessibilityUIObserver : public content::WebContentsObserver {
 public:
  AccessibilityUIObserver(content::WebContents* web_contents,
                          std::vector<std::string>* event_logs);
  ~AccessibilityUIObserver() override;

  void AccessibilityEventReceived(
      const ui::AXUpdatesAndEvents& details) override;

 private:
  raw_ptr<std::vector<std::string>> event_logs_;
};

// Manages messages sent from accessibility.js via json.
class AccessibilityUIMessageHandler : public content::WebUIMessageHandler {
 public:
  AccessibilityUIMessageHandler();

  AccessibilityUIMessageHandler(const AccessibilityUIMessageHandler&) = delete;
  AccessibilityUIMessageHandler& operator=(
      const AccessibilityUIMessageHandler&) = delete;

  ~AccessibilityUIMessageHandler() override;

  void RegisterMessages() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // Applies `mode` to `web_contents` for the lifetime of the accessibility
  // UI page.
  void SetAccessibilityModeForWebContents(content::WebContents* web_contents,
                                          ui::AXMode mode);

  void ToggleAccessibilityForWebContents(const base::Value::List& args);
  void SetGlobalFlag(const base::Value::List& args);
  void SetGlobalString(const base::Value::List& args);

  void GetRequestTypeAndFilters(const base::Value::Dict& data,
                                std::string& request_type,
                                std::string& allow,
                                std::string& allow_empty,
                                std::string& deny);
  void RequestWebContentsTree(const base::Value::List& args);
  void RequestNativeUITree(const base::Value::List& args);
  void RequestWidgetsTree(const base::Value::List& args);
  void RequestAccessibilityEvents(const base::Value::List& args);
  void Callback(const std::string&);
  void StopRecording(content::WebContents* web_contents);

  // Returns the user-set API type. or the platform's default recording type if
  // the user-set type is not supported.
  ui::AXApiType::Type GetRecordingApiType();

  // A ScopedAccessibilityMode for a page hosted in a WebContents.
  struct PageAccessibilityMode {
    base::WeakPtr<content::WebContents> web_contents;
    std::unique_ptr<content::ScopedAccessibilityMode> accessibility_mode;

    PageAccessibilityMode() = delete;
    PageAccessibilityMode(
        base::WeakPtr<content::WebContents> web_contents,
        std::unique_ptr<content::ScopedAccessibilityMode> accessibility_mode);
    PageAccessibilityMode(PageAccessibilityMode&& other) noexcept;
    PageAccessibilityMode& operator=(PageAccessibilityMode&& other) noexcept =
        default;
    ~PageAccessibilityMode();
  };

  // Accessibility modes for pages in WebContentses. The map's key is a pointer
  // to a WebContents. AccessibilityUIMessageHandler does not observe the
  // lifecycle of these WebContentses, so any additions or modifications to the
  // data for a WebContents in this mapping MUST be preceded by a sweep to erase
  // any entries for which the value's WeakPtr<WebContents> has been
  // invalidated.
  std::map<void*, PageAccessibilityMode> page_accessibility_modes_;

  // A ScopedAccessibilityMode that holds the process-wide ("global") mode flags
  // modified via the `setGlobalFlag` callback from the page. Guaranteed to hold
  // at least an instance with no mode flags set.
  std::unique_ptr<content::ScopedAccessibilityMode> process_accessibility_mode_;

  std::vector<std::string> event_logs_;
  std::unique_ptr<AccessibilityUIObserver> observer_;

  base::WeakPtrFactory<AccessibilityUIMessageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ACCESSIBILITY_UI_H_
