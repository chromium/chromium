// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_session_id_map.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace chromecast {
namespace shell {

// A small class that listens for the destruction of a WebContents, and forwards
// the event to the CastSessionIdMap with the appropriate group_id.
class CastSessionIdMap::GroupObserver : content::WebContentsObserver {
 public:
  using GroupDestroyedCallback =
      base::OnceCallback<void(base::UnguessableToken)>;

  GroupObserver(content::WebContents* web_contents,
                GroupDestroyedCallback destroyed_callback)
      : destroyed_callback_(std::move(destroyed_callback)),
        group_id_(web_contents->GetAudioGroupId()) {
    content::WebContentsObserver::Observe(web_contents);
  }

 private:
  // content::WebContentsObserver implementation:
  void WebContentsDestroyed() override {
    DCHECK(destroyed_callback_);
    content::WebContentsObserver::Observe(nullptr);
    std::move(destroyed_callback_).Run(group_id_);
  }

  GroupDestroyedCallback destroyed_callback_;
  base::UnguessableToken group_id_;
};

// static
CastSessionIdMap* CastSessionIdMap::GetInstance(
    base::SequencedTaskRunner* task_runner) {
  static base::NoDestructor<CastSessionIdMap> map(task_runner);
  return map.get();
}

void CastSessionIdMap::SetAppProperties(std::string session_id,
                                        bool is_audio_app,
                                        content::WebContents* web_contents) {
  base::UnguessableToken group_id = web_contents->GetAudioGroupId();
  // Unretained is safe here, because the singleton CastSessionIdMap never gets
  // destroyed.
  auto destroyed_callback = base::BindOnce(&CastSessionIdMap::OnGroupDestroyed,
                                           base::Unretained(GetInstance()));
  auto group_observer = std::make_unique<GroupObserver>(
      web_contents, std::move(destroyed_callback));
  SetAppPropertiesInternal(session_id, is_audio_app, group_id,
                           std::move(group_observer));
}

void CastSessionIdMap::SetGroupInfo(std::string session_id, bool is_group) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // Unretained is safe here, because the singleton CastSessionIdMap never
    // gets destroyed.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastSessionIdMap::SetGroupInfo, base::Unretained(this),
                       std::move(session_id), is_group));
    return;
  }
  group_info_mapping_.emplace(session_id, is_group);
}

CastSessionIdMap::CastSessionIdMap(base::SequencedTaskRunner* task_runner)
    : task_runner_(task_runner) {
  DCHECK(task_runner_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CastSessionIdMap::~CastSessionIdMap() = default;

void CastSessionIdMap::SetAppPropertiesInternal(
    std::string session_id,
    bool is_audio_app,
    base::UnguessableToken group_id,
    std::unique_ptr<GroupObserver> group_observer) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // Unretained is safe here, because the singleton CastSessionIdMap never
    // gets destroyed.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastSessionIdMap::SetAppPropertiesInternal,
                       base::Unretained(this), std::move(session_id),
                       is_audio_app, group_id, std::move(group_observer)));
    return;
  }

  // This check is required to bind to the current sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetSessionId(group_id.ToString()).empty());
  DCHECK(group_observer);

  DVLOG(1) << "Mapping session_id=" << session_id
           << " to group_id=" << group_id.ToString();
  auto group_data = std::make_pair(session_id, std::move(group_observer));
  mapping_.emplace(group_id.ToString(), std::move(group_data));
  application_capability_mapping_.emplace(session_id, is_audio_app);
}

std::string CastSessionIdMap::GetSessionId(const std::string& group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = mapping_.find(group_id);
  if (it != mapping_.end())
    return it->second.first;
  return std::string();
}

bool CastSessionIdMap::IsAudioOnlySession(const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = application_capability_mapping_.find(session_id);
  if (it != application_capability_mapping_.end())
    return it->second;
  return false;
}

bool CastSessionIdMap::IsGroup(const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = group_info_mapping_.find(session_id);
  if (it != group_info_mapping_.end())
    return it->second;
  return false;
}

void CastSessionIdMap::IsAudioOnlySessionAsync(
    const std::string& session_id,
    IsAudioOnlySessionAsyncCallback callback) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // Unretained is safe here, because the singleton CastSessionIdMap never
    // gets destroyed.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CastSessionIdMap::IsAudioOnlySessionAsync,
                                  base::Unretained(this), session_id,
                                  BindToCurrentSequence(std::move(callback))));
    return;
  }

  std::move(callback).Run(IsAudioOnlySession(session_id));
}

void CastSessionIdMap::OnGroupDestroyed(base::UnguessableToken group_id) {
  // Unretained is safe here, because the singleton CastSessionIdMap never gets
  // destroyed.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&CastSessionIdMap::RemoveGroupId,
                                        base::Unretained(this), group_id));
}

void CastSessionIdMap::RemoveGroupId(base::UnguessableToken group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = mapping_.find(group_id.ToString());
  if (it != mapping_.end()) {
    DVLOG(1) << "Removing mapping for session_id=" << it->second.first
             << " to group_id=" << group_id.ToString();
    auto it_app = application_capability_mapping_.find(it->second.first);
    if (it_app != application_capability_mapping_.end()) {
      application_capability_mapping_.erase(it_app);
    }
    auto it_group = group_info_mapping_.find(it->second.first);
    if (it_group != group_info_mapping_.end()) {
      group_info_mapping_.erase(it_group);
    }
    mapping_.erase(it);
  }
}

}  // namespace shell
}  // namespace chromecast
