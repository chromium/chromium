// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/discovery/media_sink_service_util.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace media_router {

void RunSinksDiscoveredCallbackOnSequence(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const OnSinksDiscoveredCallback& callback,
    std::vector<MediaSinkInternal> sinks) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(callback, std::move(sinks)));
}

void RunAvailableSinksUpdatedCallbackOnSequence(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const OnAvailableSinksUpdatedCallback& callback,
    const std::string& app_name,
    std::vector<MediaSinkInternal> available_sinks) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(callback, app_name,
                                                  std::move(available_sinks)));
}

}  // namespace media_router
