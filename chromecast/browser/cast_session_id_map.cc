// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_session_id_map.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
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

// static
void CastSessionIdMap::SetSessionId(std::string session_id,
                                    content::WebContents* web_contents) {
  base::UnguessableToken group_id = web_contents->GetAudioGroupId();
  // Unretained is safe here, because the singleton CastSessionIdMap never gets
  // destroyed.
  auto destroyed_callback = base::BindOnce(&CastSessionIdMap::OnGroupDestroyed,
                                           base::Unretained(GetInstance()));
  auto group_observer = std::make_unique<GroupObserver>(
      web_contents, std::move(destroyed_callback));
  GetInstance()->SetSessionIdInternal(session_id, group_id,
                                      std::move(group_observer));
}

// static
std::string CastSessionIdMap::GetSessionId(std::string group_id) {
  return GetInstance()->GetSessionIdInternal(group_id);
}

CastSessionIdMap::CastSessionIdMap(base::SequencedTaskRunner* task_runner)
    : task_runner_(task_runner) {
  DCHECK(task_runner_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CastSessionIdMap::~CastSessionIdMap() = default;

void CastSessionIdMap::SetSessionIdInternal(
    std::string session_id,
    base::UnguessableToken group_id,
    std::unique_ptr<GroupObserver> group_observer) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // Unretained is safe here, because the singleton CastSessionIdMap never
    // gets destroyed.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CastSessionIdMap::SetSessionIdInternal,
                                  base::Unretained(this), std::move(session_id),
                                  group_id, std::move(group_observer)));
    return;
  }

  // This check is required to bind to the current sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetSessionIdInternal(group_id.ToString()).empty());
  DCHECK(group_observer);

  DVLOG(1) << "Mapping session_id=" << session_id
           << " to group_id=" << group_id.ToString();
  auto group_data = std::make_pair(session_id, std::move(group_observer));
  mapping_.emplace(group_id.ToString(), std::move(group_data));
}

std::string CastSessionIdMap::GetSessionIdInternal(std::string group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = mapping_.find(group_id);
  if (it != mapping_.end())
    return it->second.first;
  return std::string();
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
    mapping_.erase(it);
  }
}

}  // namespace shell
}  // namespace chromecast
