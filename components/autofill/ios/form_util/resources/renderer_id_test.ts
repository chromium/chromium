// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getElementByUniqueID, setUniqueIDIfNeeded} from '//components/autofill/ios/form_util/resources/renderer_id.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

const rendererIdAPI = new CrWebApi('renderer_id_test');

rendererIdAPI.addFunction('setUniqueIDIfNeeded', setUniqueIDIfNeeded);
rendererIdAPI.addFunction('getElementByUniqueID', getElementByUniqueID);

gCrWeb.registerApi(rendererIdAPI);
