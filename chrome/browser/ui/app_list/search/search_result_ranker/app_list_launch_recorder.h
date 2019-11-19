// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_RECORDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_RECORDER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/metrics_proto/chrome_os_app_list_launch_event.pb.h"

namespace app_list {

class AppListLaunchMetricsProvider;

// TODO(crbug.com/1016655): add comments and documentation once the API has been
// finalized.
class AppListLaunchRecorder {
 public:
  // Lists all clients using thie logging system. Each project should have an
  // entry here. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  // TODO(crbug.com/1016655): add additional explanation for what it means to
  // add a separate project (eg. different IDs) once the design has been
  // finalized.
  enum class Client {
    kUnspecified = 0,
    kTesting = 1,
    kLauncher = 2,
  };

  struct LaunchInfo {
    LaunchInfo();
    LaunchInfo(const LaunchInfo& other);
    ~LaunchInfo();

    AppListLaunchRecorder::Client client;
    std::vector<std::pair<int, std::string>> hashed;
    std::vector<std::pair<int, int>> unhashed;
  };

  static AppListLaunchRecorder* GetInstance();

 private:
  friend class base::NoDestructor<AppListLaunchRecorder>;
  friend class app_list::AppListLaunchMetricsProvider;

  using EventFn = void(const LaunchInfo&);
  using LaunchEventCallback = base::RepeatingCallback<EventFn>;
  using LaunchEventCallbackList = base::CallbackList<EventFn>;
  using LaunchEventSubscription = LaunchEventCallbackList::Subscription;

  AppListLaunchRecorder();
  ~AppListLaunchRecorder();

  template <typename T>
  void Log(Client client,
           const std::vector<std::pair<T, std::string>>& hashed,
           const std::vector<std::pair<T, int>>& unhashed) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    static_assert(std::is_enum<T>::value,
                  "Non enum passed to AppListLaunchRecorder::Log");

    if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      base::PostTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&AppListLaunchRecorder::Log<T>, base::Unretained(this),
                         client, hashed, unhashed));
      return;
    }

    LaunchInfo event;
    event.client = client;
    for (const auto& pair : hashed)
      event.hashed.push_back({static_cast<int>(pair.first), pair.second});
    for (const auto& pair : unhashed)
      event.unhashed.push_back({static_cast<int>(pair.first), pair.second});

    callback_list_.Notify(event);
  }

  // Registers a callback to be invoked on a call to Log().
  std::unique_ptr<LaunchEventSubscription> RegisterCallback(
      const LaunchEventCallback& callback);

  LaunchEventCallbackList callback_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AppListLaunchRecorder);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_RECORDER_H_
