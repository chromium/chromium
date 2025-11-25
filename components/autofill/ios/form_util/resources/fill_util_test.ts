// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {registerAllChildFrames} from '//components/autofill/ios/form_util/resources/child_frame_registration_test.js';
import * as elementInferenceUtil from '//components/autofill/ios/form_util/resources/fill_element_inference.js';
import * as inferenceUtil from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import * as fillUtil from '//components/autofill/ios/form_util/resources/fill_util.js';
import {webFormElementToFormData} from '//components/autofill/ios/form_util/resources/fill_web_form.js';
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
fillApi.addFunction(
    'getOptionStringsFromElement', fillUtil.getOptionStringsFromElement);
fillApi.addFunction('getUniqueID', fillUtil.getUniqueID);
fillApi.addFunction('hasTagName', inferenceUtil.hasTagName);
fillApi.addFunction(
    'inferLabelForElement', elementInferenceUtil.inferLabelForElement);
fillApi.addFunction(
    'inferLabelFromDefinitionList',
    elementInferenceUtil.inferLabelFromDefinitionList);
fillApi.addFunction(
    'inferLabelFromDivTable', elementInferenceUtil.inferLabelFromDivTable);
fillApi.addFunction(
    'inferLabelFromEnclosingLabel',
    elementInferenceUtil.inferLabelFromEnclosingLabel);
fillApi.addFunction(
    'inferLabelFromListItem', elementInferenceUtil.inferLabelFromListItem);
fillApi.addFunction(
    'inferLabelFromPrevious', elementInferenceUtil.inferLabelFromPrevious);
fillApi.addFunction(
    'inferLabelFromTableColumn',
    elementInferenceUtil.inferLabelFromTableColumn);
fillApi.addFunction(
    'inferLabelFromTableRow', elementInferenceUtil.inferLabelFromTableRow);
fillApi.addFunction(
    'isAutofillableElement', inferenceUtil.isAutofillableElement);
fillApi.addFunction(
    'isAutofillableInputElement', inferenceUtil.isAutofillableInputElement);
fillApi.addFunction('isCheckableElement', inferenceUtil.isCheckableElement);
fillApi.addFunction('isSelectElement', inferenceUtil.isSelectElement);
fillApi.addFunction('registerAllChildFrames', registerAllChildFrames);
fillApi.addFunction('setInputElementValue', fillUtil.setInputElementValue);
fillApi.addFunction('shouldAutocomplete', fillUtil.shouldAutocomplete);
fillApi.addFunction('webFormElementToFormData', webFormElementToFormData);
// go/keep-sorted end


gCrWeb.registerApi('fill_test_api', fillApi);
