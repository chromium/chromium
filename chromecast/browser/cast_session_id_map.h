// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_SESSION_ID_MAP_H_
#define CHROMECAST_BROWSER_CAST_SESSION_ID_MAP_H_

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/unguessable_token.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {
namespace shell {

class CastSessionIdMap {
 public:
  // Retrieve the map instance. The first time this is called, a task runner can
  // be specified for the instance to run on. Any subsequent calls to this
  // function will return the same map instance, but will not change the task
  // runner.
  // This must be called for the first time on the browser main thread.
  static CastSessionIdMap* GetInstance(
      base::SequencedTaskRunner* task_runner = nullptr);
  // Map a session id to a particular group id in the provided WebContents.
  // Record whether the session is an audio only session.
  // Can be called on any thread.
  static void SetAppProperties(std::string session_id,
                               bool is_audio_app,
                               content::WebContents* web_contents);
  // Fetch the session id that is mapped to the provided group_id. Defaults to
  // empty string if the mapping is not found.
  // Must be called on the sequence for |task_runner_|.
  static std::string GetSessionId(const std::string& group_id);
  // Fetch whether the session is an audio only session based on the provided
  // session id. Defaults to false if the mapping is not found.
  // Must be called on the sequence for |task_runner_|.
  static bool IsAudioOnlySession(const std::string& session_id);

 private:
  class GroupObserver;
  friend class base::NoDestructor<CastSessionIdMap>;

  explicit CastSessionIdMap(base::SequencedTaskRunner* task_runner);
  ~CastSessionIdMap();

  // Callback for the group being destroyed.
  void OnGroupDestroyed(base::UnguessableToken group_id);
  // Removes the mapping between group_id and session_id and release the
  // GroupObserver. This must not be called in the group destructor callback,
  // because it releases the GroupObserver who owns the destuctor callback.
  void RemoveGroupId(base::UnguessableToken group_id);
  // Maps the session id for the provided group id.
  // Record whether the session is an audio only session.
  // This call be called on any thread.
  void SetAppPropertiesInternal(std::string session_id,
                                bool is_audio_app,
                                base::UnguessableToken group_id,
                                std::unique_ptr<GroupObserver> group_observer);
  // Retrieves the session id for the provided group id.
  // This must be called on the |task_runner_|.
  std::string GetSessionIdInternal(const std::string& group_id);
  // Retrieves the session is an audio only session based on the provided
  // session id.
  // This must be called on the |task_runner_|.
  bool IsAudioOnlySessionInternal(const std::string& session_id);

  base::flat_map<
      std::string,
      std::pair<std::string /* group_id */, std::unique_ptr<GroupObserver>>>
      mapping_;

  base::flat_map<std::string /* session_id */, bool /* is_audio_app */>
      application_capability_mapping_;
  base::SequencedTaskRunner* const task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CastSessionIdMap);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_SESSION_ID_MAP_H_
