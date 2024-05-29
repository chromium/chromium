// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client.h"

InspectorEvent::InspectorEvent() = default;

InspectorEvent::~InspectorEvent() = default;

InspectorEvent::InspectorEvent(InspectorEvent&& other) = default;

InspectorCommandResponse::InspectorCommandResponse() = default;

InspectorCommandResponse::~InspectorCommandResponse() = default;

InspectorCommandResponse::InspectorCommandResponse(
    InspectorCommandResponse&& other) = default;
