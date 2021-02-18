// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/projector/projector_client.h"

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_

class ProjectorClientImpl : public ash::ProjectorClient {
 public:
  ProjectorClientImpl();
  ProjectorClientImpl(const ProjectorClientImpl&) = delete;
  ProjectorClientImpl& operator=(const ProjectorClientImpl&) = delete;
  ~ProjectorClientImpl() override;

  // ash::ProjectorClient:
  void StartSpeechRecognition() override;
  void StopSpeechRecognition() override;
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_
