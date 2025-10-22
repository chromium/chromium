// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isAutofillableElement, isAutofillableInputElement, isSelectElement} from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import {getAriaDescription, getAriaLabel, getCanonicalActionForForm, getUniqueID, shouldAutocomplete} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
* @fileoverview Registers a testing-only `CrWebApi` to expose fill utils
* functions to native-side tests.
*/

const fillApi = new CrWebApi();

// go/keep-sorted start block=yes
fillApi.addFunction('getAriaDescription', getAriaDescription);
fillApi.addFunction('getAriaLabel', getAriaLabel);
fillApi.addFunction('getCanonicalActionForForm', getCanonicalActionForForm);
fillApi.addFunction('getUniqueID', getUniqueID);
fillApi.addFunction('isAutofillableElement', isAutofillableElement);
fillApi.addFunction('isAutofillableInputElement', isAutofillableInputElement);
fillApi.addFunction('isSelectElement', isSelectElement);
fillApi.addFunction('shouldAutocomplete', shouldAutocomplete);
// go/keep-sorted end

gCrWeb.registerApi('fill_test_api', fillApi);
