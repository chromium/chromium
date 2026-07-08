// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://ai-overlay-dialog/persona.js';

import type {ConversationConfig} from 'chrome-untrusted://ai-overlay-dialog/conversation.js';
import {buildSystemInstruction, processConditionals, processNumbering, processTemplate} from 'chrome-untrusted://ai-overlay-dialog/persona.js';
import {assertEquals, assertThrows} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('PersonaTest', () => {
  test('ProcessConditionalsBasic', () => {
    const data = {
      show_extra: true,
    };

    const template = 'Hello world?${show_extra}[, how are you?]{}?';
    assertEquals(
        'Hello world, how are you?', processConditionals(template, data));

    data.show_extra = false;
    assertEquals('Hello world', processConditionals(template, data));
  });

  test('ProcessConditionalsWithElse', () => {
    const data = {
      is_logged_in: true,
    };

    const template = 'Status: ?${is_logged_in}[Online]{else}[Offline]{}?';
    assertEquals('Status: Online', processConditionals(template, data));

    data.is_logged_in = false;
    assertEquals('Status: Offline', processConditionals(template, data));
  });

  test('ProcessConditionalsMissingKey', () => {
    const data = {};
    const template = '?${missing}[foo]{}?';
    assertThrows(
        () => processConditionals(template, data),
        'Key \'missing\' not found in data');
  });

  test('ProcessConditionalsMultipleElseError', () => {
    const data = {foo: true};
    const template = '?${foo}[a]{else}[b]{else}[c]{}?';
    assertThrows(
        () => processConditionals(template, data),
        'Multiple else directives found for a single conditional');
  });

  test('ProcessNumberingBasic', () => {
    const template = '#{1} then #{1} then #{1} then #{3}';
    assertEquals('1 then 2 then 3 then 1', processNumbering(template));
  });

  test('ProcessNumberingMixed', () => {
    const template = 'Group A: #{1}, #{1}. Group B: #{2}, #{2}. Group A: #{1}';
    assertEquals(
        'Group A: 1, 2. Group B: 1, 2. Group A: 3', processNumbering(template));
  });

  test('ProcessTemplateBasic', () => {
    const data = {name: 'Alice', age: 30};
    const template = 'Hello ${name}, you are ${age} years old.';
    assertEquals(
        'Hello Alice, you are 30 years old.', processTemplate(template, data));
  });

  test('ProcessNumberingIntegration', () => {
    const data = {
      show_extra: true,
      name: 'User',
    };
    // Testing interaction with conditionals and variables.
    const template =
        '?${show_extra}[Item #{1}: Extra]{else}[Item #{1}: Basic]{}? - ' +
        '${name} - #{2}';

    // show_extra: true => "Item 1: Extra - User - 1"
    assertEquals('Item 1: Extra - User - 1', processTemplate(template, data));

    // show_extra: false => "Item 1: Basic - User - 1"
    data.show_extra = false;
    assertEquals('Item 1: Basic - User - 1', processTemplate(template, data));
  });

  test('ProcessTemplateFullIntegration', () => {
    const data = {
      is_premium: true,
      user_name: 'Bob',
      has_discount: false,
    };

    const template = 'Welcome ${user_name}! ' +
        '?${is_premium}[' +
        'Premium Member. Item #{1}: Gold Badge, Item #{1}: Priority Support.' +
        ']{else}[' +
        'Basic Member. Item #{1}: Profile Access.' +
        ']{}? ' +
        'Status: ?${has_discount}[Discount Applied]{else}[No Discounts]{}? ' +
        'Final check: #{1}.';

    const expectedPremium = 'Welcome Bob! ' +
        'Premium Member. Item 1: Gold Badge, Item 2: Priority Support. ' +
        'Status: No Discounts ' +
        'Final check: 3.';
    assertEquals(expectedPremium, processTemplate(template, data));

    data.is_premium = false;
    data.has_discount = true;
    // Note: Since numbering is processed after conditionals, #{1} in the "else"
    // block becomes the first occurrence.
    const expectedBasic = 'Welcome Bob! ' +
        'Basic Member. Item 1: Profile Access. ' +
        'Status: Discount Applied ' +
        'Final check: 2.';
    assertEquals(expectedBasic, processTemplate(template, data));
  });

  test('BuildSystemInstructionUsePersonaTrue', () => {
    const config: ConversationConfig = {
      system_instruction:
          'Assistant persona details: ${persona}. Call us: ${nameList}.',
      persona: {
        id: '1',
        name: 'Chef Billy',
        nicknames: ['Billy', 'Bill'],
        persona: 'You are Chef Billy, an elite French chef.',
        voice: 'Aoede',
      },
      api_config: {endpointUrl: '', model: '', apiKey: ''},
    };
    const expected =
        'Assistant persona details: You are Chef Billy, an elite French ' +
        'chef.. Call us: Billy, Bill.';
    assertEquals(expected, buildSystemInstruction(config));
  });

  test('BuildSystemInstructionUsePersonaFalse', () => {
    const config: ConversationConfig = {
      system_instruction:
          'Assistant persona details: ${persona}. Call us: ${nameList}.',
      persona: {
        id: 'generic',
        name: 'Chrome',
        nicknames: [],
        persona: '',
        voice: 'Aoede',
      },
      api_config: {endpointUrl: '', model: '', apiKey: ''},
    };
    const expected = 'Assistant persona details: . Call us: Chrome.';
    assertEquals(expected, buildSystemInstruction(config));
  });
});
