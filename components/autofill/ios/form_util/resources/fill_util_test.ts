// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {getUniqueID} from '//components/autofill/ios/form_util/resources/fill_util.js';

/**
* @fileoverview Registers a testing-only `CrWebApi` to expose fill utils
* functions to native-side tests.
*/

const fillApi = new CrWebApi();

fillApi.addFunction('getUniqueID', getUniqueID);

gCrWeb.registerApi('fill_test_api', fillApi);