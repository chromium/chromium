// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_GUIDED_BROWSING_BROWSER_METRICS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_GUIDED_BROWSING_BROWSER_METRICS_H_

// To generate Autofill Assistant guided browsing metrics.
namespace autofill_assistant::guided_browsing {

// The actions performed to grant the camera permission.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.guided_browsing.metrics)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: CameraPermissionEvent
//
// This enum is used in histograms, do not remove/renumber entries. Only add
// at the end and update kMaxValue. Also remember to update the
// AutofillAssistantGuidedBrowsingCameraPermissionEvent enum listing in
// tools/metrics/histograms/enums.xml.
enum class CameraPermissionEvent {
  CHECKING_CAMERA_PERMISSION = 0,
  ALREADY_HAD_CAMERA_PERMISSION = 1,
  CAN_PROMPT_CAMERA_PERMISSION = 2,
  CANNOT_PROMPT_CAMERA_PERMISSION = 3,
  CAMERA_PERMISSION_GRANTED_VIA_PROMPT = 4,
  CAMERA_PERMISSION_GRANTED_VIA_SETTINGS = 5,

  kMaxValue = CAMERA_PERMISSION_GRANTED_VIA_SETTINGS
};

// The actions performed to grant the read images permission.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.guided_browsing.metrics)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ReadImagesPermissionEvent
//
// This enum is used in histograms, do not remove/renumber entries. Only add
// at the end and update kMaxValue. Also remember to update the
// AutofillAssistantGuidedBrowsingReadImagesPermissionEvent enum listing in
// tools/metrics/histograms/enums.xml.
enum class ReadImagesPermissionEvent {
  CHECKING_READ_IMAGES_PERMISSION = 0,
  ALREADY_HAD_READ_IMAGES_PERMISSION = 1,
  CAN_PROMPT_READ_IMAGES_PERMISSION = 2,
  CANNOT_PROMPT_READ_IMAGES_PERMISSION = 3,
  READ_IMAGES_PERMISSION_GRANTED_VIA_PROMPT = 4,
  READ_IMAGES_PERMISSION_GRANTED_VIA_SETTINGS = 5,

  kMaxValue = READ_IMAGES_PERMISSION_GRANTED_VIA_SETTINGS
};

// The events performed by Parse Single Tag XML action.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.guided_browsing.metrics)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ParseSingleTagXmlActionEvent
//
// This enum is used in histograms, do not remove/renumber entries. Only add
// at the end and update kMaxValue. Also remember to update the
// AutofillAssistantGuidedBrowsingParseSingleTagXmlActionEvent enum listing in
// tools/metrics/histograms/enums.xml.
enum class ParseSingleTagXmlActionEvent {
  SINGLE_TAG_XML_PARSE_START = 0,
  SINGLE_TAG_XML_PARSE_SIGNED_DATA = 1,
  SINGLE_TAG_XML_PARSE_INCORRECT_DATA = 2,
  SINGLE_TAG_XML_PARSE_SOME_KEY_MISSING = 3,
  SINGLE_TAG_XML_PARSE_SUCCESS = 4,

  kMaxValue = SINGLE_TAG_XML_PARSE_SUCCESS
};

}  // namespace autofill_assistant::guided_browsing

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_GUIDED_BROWSING_BROWSER_METRICS_H_
