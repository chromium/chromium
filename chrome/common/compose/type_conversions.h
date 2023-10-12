// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_COMPOSE_TYPE_CONVERSIONS_H_
#define CHROME_COMMON_COMPOSE_TYPE_CONVERSIONS_H_

#include "chrome/common/compose/compose.mojom.h"
#include "components/compose/proto/compose_metadata.pb.h"

compose_proto::ComposeLength ComposeLength(compose::mojom::Length length);
compose_proto::ComposeTone ComposeTone(compose::mojom::Tone tone);

#endif  // CHROME_COMMON_COMPOSE_TYPE_CONVERSIONS_H_
