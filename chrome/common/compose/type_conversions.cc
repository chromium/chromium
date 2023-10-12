// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/compose/type_conversions.h"

compose_proto::ComposeLength ComposeLength(compose::mojom::Length length) {
  switch (length) {
    case compose::mojom::Length::kShorter:
      return compose_proto::ComposeLength::COMPOSE_SHORTER;
    case compose::mojom::Length::kLonger:
      return compose_proto::ComposeLength::COMPOSE_LONGER;
    case compose::mojom::Length::kUnset:
    default:
      return compose_proto::ComposeLength::COMPOSE_UNSPECIFIED_LENGTH;
  }
}

compose_proto::ComposeTone ComposeTone(compose::mojom::Tone tone) {
  switch (tone) {
    case compose::mojom::Tone::kCasual:
      return compose_proto::ComposeTone::COMPOSE_INFORMAL;
    case compose::mojom::Tone::kFormal:
      return compose_proto::ComposeTone::COMPOSE_FORMAL;
    case compose::mojom::Tone::kUnset:
    default:
      return compose_proto::ComposeTone::COMPOSE_UNSPECIFIED_TONE;
  }
}
