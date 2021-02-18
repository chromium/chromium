// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include "ash/public/cpp/projector/projector_controller.h"

ProjectorClientImpl::ProjectorClientImpl() {
  ash::ProjectorController::Get()->SetClient(this);
}

ProjectorClientImpl::~ProjectorClientImpl() = default;

void ProjectorClientImpl::StartSpeechRecognition() {
  // TODO(yilkal): Implement the connection to speech recognition.
}

void ProjectorClientImpl::StopSpeechRecognition() {
  // TODO(yilkal): Implement method to stop speech recognition.
}
