// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_TYPES_H_
#define COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_TYPES_H_

namespace content_creation {

// All supported reaction types. Corresponding |@IntDef| definitions are
// generated on the Java side at build time.
// Keep in sync with the enums.xml equivalent at
//     tools/metrics/histograms/enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.content_creation.reactions)
enum class ReactionType {
  CLAP = 0,
  EMOTIONAL = 1,
  EYES = 2,
  FIRE = 3,
  GRIN = 4,
  SURPRISE = 5,
  THANKS = 6,
  UNSURE = 7,
  HEART = 8,
  LAUGH_CRY = 9,

  MAX_VALUE = LAUGH_CRY
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_TYPES_H_