// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {GetRelatedWebsiteSetsResponse, RelatedWebsiteSet} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {SiteType} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

export const SAMPLE_RELATED_WEBSITE_SET: RelatedWebsiteSet = {
  primarySite: 'set1-primary.example',
  memberSites: [
    {
      site: 'set1-associated1.example',
      type: SiteType.kAssociated,
    },
    {
      site: 'set1-associated2.example',
      type: SiteType.kAssociated,
    },
    {
      site: 'set1-service1.example',
      type: SiteType.kService,
    },
    {
      site: 'set1-service2.example',
      type: SiteType.kService,
    },
  ],
  managedByEnterprise: false,
};

const SAMPLE_RELATED_WEBSITE_SET2: RelatedWebsiteSet = {
  primarySite: 'set2-primary.example',
  memberSites: [
    {
      site: 'set2-associated1.example',
      type: SiteType.kAssociated,
    },
    {
      site: 'set2-associated2.example',
      type: SiteType.kAssociated,
    },
    {
      site: 'set2-service1.example',
      type: SiteType.kService,
    },
    {
      site: 'set2-service2.example',
      type: SiteType.kService,
    },
  ],
  managedByEnterprise: false,
};

const SAMPLE_RELATED_WEBSITE_SET3: RelatedWebsiteSet = {
  primarySite: 'set3-primary.example',
  memberSites: [
    {
      site: 'set3-associated1.example',
      type: SiteType.kAssociated,
    },
    {
      site: 'set3-associated2.example',
      type: SiteType.kAssociated,
    },
    {
      site: 'set3-service1.example',
      type: SiteType.kService,
    },
    {
      site: 'set3-service2.example',
      type: SiteType.kService,
    },
  ],
  managedByEnterprise: false,
};

export const SAMPLE_RELATED_WEBSITE_SET_MANAGED_BY_ENTERPRISE:
    RelatedWebsiteSet = {
  primarySite: 'set-managed-by-enterprise-primary.example',
  memberSites: [
    {
      site: 'set-managed-by-enterprise--associated1.example',
      type: SiteType.kAssociated,
    },
  ],
  managedByEnterprise: true,
};

export const SAMPLE_RELATED_WEBSITE_SETS = [
  SAMPLE_RELATED_WEBSITE_SET,
  SAMPLE_RELATED_WEBSITE_SET2,
  SAMPLE_RELATED_WEBSITE_SET3,
];

export const GetRelatedWebsiteSetsResponseForTest:
    GetRelatedWebsiteSetsResponse = {
      relatedWebsiteSets: SAMPLE_RELATED_WEBSITE_SETS,
    };
