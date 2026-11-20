// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OnDeviceAiCapability} from 'chrome://settings/lazy_load.js';
import {OnDeviceModelCategory, OnDeviceModelStatus} from 'chrome://settings/lazy_load.js';

// These helpers return a new object on every call, so that tests never share
// state with each other or with the element under test.

export function createGemmaCapability(
    overrides: Partial<OnDeviceAiCapability> = {}): OnDeviceAiCapability {
  return {
    id: 1,
    status: OnDeviceModelStatus.READY,
    category: OnDeviceModelCategory.GENERAL_PURPOSE,
    categoryTitle: 'General-purpose writing',
    description: 'Helps sites write, rewrite, and summarize text',
    modelName: 'Gemma 4',
    size: '2.0 GB',
    details: [
      'Helps you draft, rephrase, and organize text on sites that use it',
      'Handles many tasks, so one download covers multiple features',
    ],
    ...overrides,
  };
}

export function createAsrCapability(
    overrides: Partial<OnDeviceAiCapability> = {}): OnDeviceAiCapability {
  return {
    id: 2,
    status: OnDeviceModelStatus.READY,
    category: OnDeviceModelCategory.LIVE_TRANSCRIPTION,
    categoryTitle: 'Live transcription',
    description: 'Captions and transcribes audio',
    modelName: 'Live Caption and Transcription',
    size: '3.0 GB',
    details: ['Works on sites that support live audio transcription'],
    ...overrides,
  };
}
