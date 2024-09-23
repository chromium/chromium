// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SESSION_H_
#define CHROME_TEST_CHROMEDRIVER_SESSION_H_

#include <list>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/geoposition.h"
#include "chrome/test/chromedriver/chrome/network_conditions.h"
#include "chrome/test/chromedriver/chrome/scoped_temp_dir_with_retry.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/command_listener.h"
#include "chrome/test/chromedriver/prompt_behavior.h"

// Controls whether ChromeDriver operates in W3C mode (when true) by default
// or legacy mode (when false).
static const bool kW3CDefault = true;

class Chrome;
class Status;
class WebDriverLog;
class WebView;

struct FrameInfo {
  FrameInfo(const std::string& parent_frame_id,
            const std::string& frame_id,
            const std::string& chromedriver_frame_id);

  std::string parent_frame_id;
  std::string frame_id;
  std::string chromedriver_frame_id;
};

struct InputCancelListEntry {
  InputCancelListEntry(base::Value::Dict* input_state,
                       const MouseEvent* mouse_event,
                       const TouchEvent* touch_event,
                       const KeyEvent* key_event);
  InputCancelListEntry(InputCancelListEntry&& other);
  ~InputCancelListEntry();

  raw_ptr<base::Value::Dict> input_state;
  std::unique_ptr<MouseEvent> mouse_event;
  std::unique_ptr<TouchEvent> touch_event;
  std::unique_ptr<KeyEvent> key_event;
};

typedef base::RepeatingCallback<void(std::string /*payload*/)> SendTextFunc;

typedef base::RepeatingCallback<void()> CloseFunc;

struct BidiConnection {
  BidiConnection(int connection_id,
                 SendTextFunc send_response,
                 CloseFunc close_connection);
  BidiConnection(BidiConnection&& other);
  ~BidiConnection();
  BidiConnection& operator=(BidiConnection&& other);
  int connection_id;
  SendTextFunc send_response;
  CloseFunc close_connection;
};

struct Session {
  static const base::TimeDelta kDefaultImplicitWaitTimeout;
  static const base::TimeDelta kDefaultPageLoadTimeout;
  static const base::TimeDelta kDefaultScriptTimeout;
  // Non-standard timeouts
  static const base::TimeDelta kDefaultBrowserStartupTimeout;
  // BiDi channels
  static const char kChannelSuffix[];
  static const char kNoChannelSuffix[];
  static const char kBlockingChannelSuffix[];

  explicit Session(const std::string& id);
  Session(const std::string& id, std::unique_ptr<Chrome> chrome);
  Session(const std::string& id, const std::string& host);
  ~Session();

  Status GetTargetWindow(WebView** web_view);

  void SwitchToTopFrame();
  void SwitchToParentFrame();
  void SwitchToSubFrame(const std::string& frame_id,
                        const std::string& chromedriver_frame_id);
  std::string GetCurrentFrameId() const;
  std::vector<WebDriverLog*> GetAllLogs() const;

  Status OnBidiResponse(base::Value::Dict payload);
  void AddBidiConnection(int connection_id,
                         SendTextFunc send_response,
                         CloseFunc close_connection);
  void RemoveBidiConnection(int connection_id);
  void CloseAllConnections();

  const std::string id;
  bool w3c_compliant;
  bool web_socket_url = false;
  bool quit;
  bool detach;
  bool awaiting_bidi_response = false;
  std::unique_ptr<Chrome> chrome;
  std::string window;
  std::string bidi_mapper_web_view_id;
  int sticky_modifiers;
  // List of input sources for each active input. Everytime a new input source
  // is added, there must be a corresponding entry made in `input_state_table`.
  base::Value::List active_input_sources;
  // Map between input id and input source state for the corresponding input
  // source. One entry for each item in `active_input_sources`.
  base::Value::Dict input_state_table;
  // List of actions for Release Actions command.
  std::vector<InputCancelListEntry> input_cancel_list;
  // List of |FrameInfo|s for each frame to the current target frame from the
  // first frame element in the root document. If target frame is window.top,
  // this list will be empty.
  std::list<FrameInfo> frames;
  // Download directory that the user specifies. Used only in headless mode.
  // Defaults to current directory in headless mode if no directory specified
  std::unique_ptr<std::string> headless_download_directory;
  WebPoint mouse_position;
  MouseButton pressed_mouse_button;
  base::TimeDelta implicit_wait;
  base::TimeDelta page_load_timeout;
  base::TimeDelta script_timeout;
  std::optional<std::string> prompt_text;
  std::unique_ptr<Geoposition> overridden_geoposition;
  std::unique_ptr<DeviceMetrics> overridden_device_metrics;
  std::unique_ptr<NetworkConditions> overridden_network_conditions;
  std::string orientation_type;
  // Logs that populate from DevTools events.
  std::vector<std::unique_ptr<WebDriverLog>> devtools_logs;
  std::unique_ptr<WebDriverLog> driver_log;
  ScopedTempDirWithRetry temp_dir;
  std::unique_ptr<base::Value::Dict> capabilities;
  // |command_listeners| should be declared after |chrome|. When the |Session|
  // is destroyed, |command_listeners| should be freed first, since some
  // |CommandListener|s might be |CommandListenerProxy|s that forward to
  // |DevToolsEventListener|s owned by |chrome|.
  std::vector<std::unique_ptr<CommandListener>> command_listeners;
  bool strict_file_interactability;

  PromptBehavior unhandled_prompt_behavior = PromptBehavior(kW3CDefault);
  int click_count;
  base::TimeTicks mouse_click_timestamp;
  std::string host;

 private:
  void SwitchFrameInternal(bool for_top_frame);

  std::vector<BidiConnection> bidi_connections_;
};

Session* GetThreadLocalSession();

void SetThreadLocalSession(std::unique_ptr<Session> new_session);

namespace internal {
Status SplitChannel(std::string* channel,
                    int* connection_id,
                    std::string* suffix);
}

#endif  // CHROME_TEST_CHROMEDRIVER_SESSION_H_
