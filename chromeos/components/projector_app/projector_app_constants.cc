// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/projector_app_constants.h"

namespace chromeos {

const char kChromeUIProjectorAppHost[] = "projector";

// content::WebUIDataSource::Create() requires trailing slash.
const char kChromeUIUntrustedProjectorAppUrl[] =
    "chrome-untrusted://projector/";
const char kChromeUIUntrustedProjectorPwaUrl[] =
    "https://projector.apps.chrome";

const char kChromeUITrustedProjectorUrl[] = "chrome://projector/";
const char kChromeUITrustedProjectorAppUrl[] = "chrome://projector/app/";
const char kChromeUITrustedProjectorSelfieCamUrl[] =
    "chrome://projector/selfie_cam/selfie_cam.html";
const char kChromeUITrustedAnnotatorUrl[] =
    "chrome://projector/annotator/annotator_embedder.html";
const char kChromeUITrustedProjectorSwaAppId[] =
    "fgnpbdobngpkonkajbmelfhjkemaddhp";
}  // namespace chromeos
