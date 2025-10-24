// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as inferenceUtil from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import * as fillUtil from '//components/autofill/ios/form_util/resources/fill_util.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
* @fileoverview Registers a testing-only `CrWebApi` to expose fill utils
* functions to native-side tests.
*/

const fillApi = new CrWebApi();

// go/keep-sorted start block=yes
fillApi.addFunction(
    'combineAndCollapseWhitespace', inferenceUtil.combineAndCollapseWhitespace);
fillApi.addFunction('getAriaDescription', fillUtil.getAriaDescription);
fillApi.addFunction('getAriaLabel', fillUtil.getAriaLabel);
fillApi.addFunction(
    'getCanonicalActionForForm', fillUtil.getCanonicalActionForForm);
fillApi.addFunction('getUniqueID', fillUtil.getUniqueID);
fillApi.addFunction('hasTagName', inferenceUtil.hasTagName);
fillApi.addFunction(
    'isAutofillableElement', inferenceUtil.isAutofillableElement);
fillApi.addFunction(
    'isAutofillableInputElement', inferenceUtil.isAutofillableInputElement);
fillApi.addFunction('isCheckableElement', inferenceUtil.isCheckableElement);
fillApi.addFunction('isSelectElement', inferenceUtil.isSelectElement);
fillApi.addFunction('setInputElementValue', fillUtil.setInputElementValue);
fillApi.addFunction('shouldAutocomplete', fillUtil.shouldAutocomplete);
// go/keep-sorted end


gCrWeb.registerApi('fill_test_api', fillApi);
