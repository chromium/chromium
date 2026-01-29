// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {formSubmitted} from '//components/autofill/ios/form_util/resources/fill_web_form.js';
import {getIframeElements} from '//components/autofill/ios/form_util/resources/form_utils.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
* @fileoverview Registers a testing-only `CrWebApi` to expose form utils
* functions to native-side tests.
*/

const formApi = new CrWebApi('form_test_api');

// go/keep-sorted start block=yes
formApi.addFunction('formSubmitted', formSubmitted);
formApi.addFunction('getIframeElements', getIframeElements);
// go/keep-sorted end

gCrWeb.registerApi(formApi);
