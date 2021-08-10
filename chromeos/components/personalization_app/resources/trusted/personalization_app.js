// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview the main entry point for the Personalization SWA. This imports
 * all of the necessary global modules and polymer elements to bootstrap the
 * page.
 */

import '/strings.m.js';
import './local_images_element.js';
import './personalization_router_element.js';
import './personalization_toast_element.js';
import './wallpaper_breadcrumb_element.js';
import './wallpaper_collections_element.js';
import './wallpaper_error_element.js';
import './wallpaper_images_element.js';
import './wallpaper_selected_element.js';
import './styles.js';
import {onMessageReceived} from './personalization_message_handler.js';
import {emptyState} from './personalization_reducers.js';
import {PersonalizationStore} from './personalization_store.js';

PersonalizationStore.getInstance().init(emptyState());

window.addEventListener('message', onMessageReceived);
