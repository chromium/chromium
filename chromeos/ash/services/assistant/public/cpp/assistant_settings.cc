// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/public/cpp/assistant_settings.h"

namespace ash::assistant {

namespace {

AssistantSettings* g_instance = nullptr;

}  // namespace

SpeakerIdEnrollmentClient::SpeakerIdEnrollmentClient() = default;
SpeakerIdEnrollmentClient::~SpeakerIdEnrollmentClient() = default;

mojo::PendingRemote<libassistant::mojom::SpeakerIdEnrollmentClient>
SpeakerIdEnrollmentClient::BindNewPipeAndPassRemote() {
  return client_.BindNewPipeAndPassRemote();
}

void SpeakerIdEnrollmentClient::ResetReceiver() {
  client_.reset();
}

// static
AssistantSettings* AssistantSettings::Get() {
  return g_instance;
}

AssistantSettings::AssistantSettings() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AssistantSettings::~AssistantSettings() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash::assistant
