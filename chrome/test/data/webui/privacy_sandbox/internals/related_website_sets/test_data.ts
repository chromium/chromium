// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {RelatedWebsiteSet} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {SiteType} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

export const set1: RelatedWebsiteSet = {
  primarySite: 'set1-primary.example',
  memberSites: [
    {
      site: 'set1-associated1.example',
      type: SiteType.ASSOCIATED,
    },
    {
      site: 'set1-associated2.example',
      type: SiteType.ASSOCIATED,
    },
    {
      site: 'set1-service1.example',
      type: SiteType.SERVICE,
    },
    {
      site: 'set1-service2.example',
      type: SiteType.SERVICE,
    },
  ],
};

const set2: RelatedWebsiteSet = {
  primarySite: 'set2-primary.example',
  memberSites: [
    {
      site: 'set2-associated1.example',
      type: SiteType.ASSOCIATED,
    },
    {
      site: 'set2-associated2.example',
      type: SiteType.ASSOCIATED,
    },
    {
      site: 'set2-service1.example',
      type: SiteType.SERVICE,
    },
    {
      site: 'set2-service2.example',
      type: SiteType.SERVICE,
    },
  ],
};

const set3: RelatedWebsiteSet = {
  primarySite: 'set3-primary.example',
  memberSites: [
    {
      site: 'set3-associated1.example',
      type: SiteType.ASSOCIATED,
    },
    {
      site: 'set3-associated2.example',
      type: SiteType.ASSOCIATED,
    },
    {
      site: 'set3-service1.example',
      type: SiteType.SERVICE,
    },
    {
      site: 'set3-service2.example',
      type: SiteType.SERVICE,
    },
  ],
};

export const dummySets: RelatedWebsiteSet[] = [set1, set2, set3];
