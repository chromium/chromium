// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const NAME_COLUMN = 'name';
export const TOGGLE_COLUMN = 'toggle';

export enum OptionalPolicyColumn {
  VALUE = 'value',
  SOURCE = 'source',
  SCOPE = 'scope',
  LEVEL = 'level',
  STATUS = 'status',
}

export const OPTIONAL_POLICY_COLUMNS: readonly OptionalPolicyColumn[] = [
  OptionalPolicyColumn.VALUE,
  OptionalPolicyColumn.SOURCE,
  OptionalPolicyColumn.SCOPE,
  OptionalPolicyColumn.LEVEL,
  OptionalPolicyColumn.STATUS,
];
