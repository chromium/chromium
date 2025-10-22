// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hasTagName, isAutofillableElement, isAutofillableInputElement, isSelectElement} from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import * as fillUtil from '//components/autofill/ios/form_util/resources/fill_util.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
* @fileoverview Registers a testing-only `CrWebApi` to expose fill utils
* functions to native-side tests.
*/

const fillApi = new CrWebApi();

// go/keep-sorted start block=yes
fillApi.addFunction('getAriaDescription', fillUtil.getAriaDescription);
fillApi.addFunction('getAriaLabel', fillUtil.getAriaLabel);
fillApi.addFunction(
    'getCanonicalActionForForm', fillUtil.getCanonicalActionForForm);
fillApi.addFunction('getUniqueID', fillUtil.getUniqueID);
fillApi.addFunction('hasTagName', hasTagName);
fillApi.addFunction('isAutofillableElement', isAutofillableElement);
fillApi.addFunction('isAutofillableInputElement', isAutofillableInputElement);
fillApi.addFunction('isSelectElement', isSelectElement);
fillApi.addFunction('setInputElementValue', fillUtil.setInputElementValue);
fillApi.addFunction('shouldAutocomplete', fillUtil.shouldAutocomplete);
// go/keep-sorted end


gCrWeb.registerApi('fill_test_api', fillApi);
