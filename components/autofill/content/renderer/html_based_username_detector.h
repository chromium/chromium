// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_HTML_BASED_USERNAME_DETECTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_HTML_BASED_USERNAME_DETECTOR_H_

#include <map>
#include <vector>

#include "components/autofill/core/common/password_form.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"

namespace autofill {

// The detector's cache is a map from a |unique_renderer_id| to the list of
// predictions for the given form (in the order of decreasing reliability).
using UsernameDetectorCache = std::map<uint32_t, std::vector<uint32_t>>;

// Classifier for getting username field by analyzing HTML attribute values.
// The algorithm looks for words that are likely to point to username field (ex.
// "username", "loginid" etc.), in the attribute values. When the first match is
// found, the currently analyzed field is saved in |username_element|, and the
// algorithm ends. By searching for words in order of their probability to be
// username words, it is sure that the first match will also be the best one.
// If detector's outcome for the given form is cached in
// |username_detector_cache|, then |username_element| is set based on the cached
// data. Otherwise, the detector will be run and the outcome will be saved to
// the cache. The function returns a reference to the vector of predictions,
// which is stored in the cache.
const std::vector<uint32_t>& GetPredictionsFieldBasedOnHtmlAttributes(
    const std::vector<blink::WebFormControlElement>& all_control_elements,
    const FormData& form_data,
    UsernameDetectorCache* username_detector_cache);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_HTML_BASED_USERNAME_DETECTOR_H_
