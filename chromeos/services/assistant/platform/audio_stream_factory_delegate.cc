// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_stream_factory_delegate.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"

namespace chromeos {
namespace assistant {

void DefaultAudioStreamFactoryDelegate::RequestAudioStreamFactory(
    Callback callback) {
  mojo::PendingRemote<audio::mojom::StreamFactory> result;
  AssistantClient::Get()->RequestAudioStreamFactory(
      result.InitWithNewPipeAndPassReceiver());

  // Note we're calling the callback asynchronous because we do not want the
  // callers to rely on the fact that the callback might be called synchrously.
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

scoped_refptr<base::SequencedTaskRunner>
DefaultAudioStreamFactoryDelegate::task_runner() {
  return base::SequencedTaskRunnerHandle::Get();
}

}  // namespace assistant
}  // namespace chromeos
