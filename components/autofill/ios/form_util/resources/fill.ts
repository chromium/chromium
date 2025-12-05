// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//components/autofill/ios/form_util/resources/fill_util.js';

import type {AutofillFormData} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {webFormElementToFormData} from '//components/autofill/ios/form_util/resources/fill_web_form.js';
import {gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// This file provides methods used to fill forms in JavaScript.

// Requires functions from form.ts and child_frame_registration_lib.ts.

/**
 * Returns a serialized version of |form| to send to the host on form
 * submission.
 *
 * @param form The form to serialize.
 * @return a JSON encoded version of |form|
 */
gCrWebLegacy.fill.autofillSubmissionData = function(form: HTMLFormElement):
    AutofillFormData {
      const formData = new gCrWebLegacy['common'].JSONSafeObject();
      webFormElementToFormData(window, form, null, formData);
      return formData;
    };
