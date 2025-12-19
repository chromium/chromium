// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {expect} from '//webui-test/chai.js';
import type {ActivateEndEvent, ActivateStartEvent, AppCommandEndEvent, AppCommandStartEvent, HistoryEvent, InstallEndEvent, InstallStartEvent, LoadPolicyEndEvent, LoadPolicyStartEvent, MergedInstallEvent, MergedUpdaterProcessEvent, PersistedDataEvent, PostRequestEndEvent, PostRequestStartEvent, QualifyEndEvent, QualifyStartEvent, UninstallEndEvent, UninstallStartEvent, UpdateEndEvent, UpdaterProcessEndEvent, UpdaterProcessStartEvent, UpdateStartEvent} from 'chrome://updater/event_history.js';
import {deduplicateEvents, mergeEvents, parseEvent, UpdaterProcessMap} from 'chrome://updater/event_history.js';

suite('parseEvent', () => {
  test('should parse a valid INSTALL START event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'INSTALL',
      'eventId': 'event1',
      'deviceUptime': '12345',
      'pid': 123,
      'processToken': 'token1',
      'bound': 'START',
      'appId': '{app1}',
    };
    const event = parseEvent(message) as InstallStartEvent;
    expect(event.eventType).to.equal('INSTALL');
    expect(event.bound).to.equal('START');
    expect(event.eventId).to.equal('event1');
    expect(event.deviceUptime).to.equal(12345);
    expect(event.pid).to.equal(123);
    expect(event.processToken).to.equal('token1');
    expect(event.appId).to.equal('{app1}');
    expect(event.errors).to.deep.equal([]);
  });

  test('should parse a valid INSTALL END event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'INSTALL',
      'eventId': 'event1',
      'deviceUptime': '123456',
      'pid': 123,
      'processToken': 'token1',
      'bound': 'END',
      'version': '1.0',
    };
    const event = parseEvent(message) as InstallEndEvent;
    expect(event.eventType).to.equal('INSTALL');
    expect(event.bound).to.equal('END');
    expect(event.eventId).to.equal('event1');
    expect(event.version).to.equal('1.0');
    expect(event.errors).to.deep.equal([]);
  });

  test('should parse a valid UNINSTALL START event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'UNINSTALL',
      'eventId': 'event2',
      'deviceUptime': '22345',
      'pid': 124,
      'processToken': 'token2',
      'bound': 'START',
      'appId': '{app2}',
      'version': '1.0',
      'reason': 'UNINSTALLED',
    };
    const event = parseEvent(message) as UninstallStartEvent;
    expect(event.eventType).to.equal('UNINSTALL');
    expect(event.bound).to.equal('START');
    expect(event.eventId).to.equal('event2');
    expect(event.appId).to.equal('{app2}');
    expect(event.version).to.equal('1.0');
    expect(event.reason).to.equal('UNINSTALLED');
    expect(event.errors).to.deep.equal([]);
  });

  test(
      'should parse an UNINSTALL START event with arbitrary reason string',
      () => {
        const message: Record<string, unknown> = {
          'eventType': 'UNINSTALL',
          'eventId': 'event2',
          'deviceUptime': '22345',
          'pid': 124,
          'processToken': 'token2',
          'bound': 'START',
          'appId': '{app2}',
          'version': '1.0',
          'reason': 'SOME_OTHER_REASON',
        };
        const event = parseEvent(message) as UninstallStartEvent;
        expect(event.reason).to.equal('SOME_OTHER_REASON');
      });

  test('should parse a valid UNINSTALL END event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'UNINSTALL',
      'eventId': 'event2',
      'deviceUptime': '223456',
      'pid': 124,
      'processToken': 'token2',
      'bound': 'END',
    };
    const event = parseEvent(message) as UninstallEndEvent;
    expect(event.eventType).to.equal('UNINSTALL');
    expect(event.bound).to.equal('END');
    expect(event.eventId).to.equal('event2');
    expect(event.errors).to.deep.equal([]);
  });

  test('should parse a valid UPDATE START event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'UPDATE',
      'eventId': 'event3',
      'deviceUptime': '32345',
      'pid': 125,
      'processToken': 'token3',
      'bound': 'START',
      'appId': '{app3}',
      'priority': 'BACKGROUND',
    };
    const event = parseEvent(message) as UpdateStartEvent;
    expect(event.eventType).to.equal('UPDATE');
    expect(event.bound).to.equal('START');
    expect(event.eventId).to.equal('event3');
    expect(event.appId).to.equal('{app3}');
    expect(event.priority).to.equal('BACKGROUND');
    expect(event.errors).to.deep.equal([]);
  });

  test('should parse a valid UPDATE END event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'UPDATE',
      'eventId': 'event3',
      'deviceUptime': '323456',
      'pid': 125,
      'processToken': 'token3',
      'bound': 'END',
      'outcome': 'UPDATED',
      'nextVersion': '2.0',
    };
    const event = parseEvent(message) as UpdateEndEvent;
    expect(event.eventType).to.equal('UPDATE');
    expect(event.bound).to.equal('END');
    expect(event.eventId).to.equal('event3');
    expect(event.outcome).to.equal('UPDATED');
    expect(event.nextVersion).to.equal('2.0');
    expect(event.errors).to.deep.equal([]);
  });

  test('should parse an UPDATE END event with arbitrary outcome string', () => {
    const message: Record<string, unknown> = {
      'eventType': 'UPDATE',
      'eventId': 'event3',
      'deviceUptime': '323456',
      'pid': 125,
      'processToken': 'token3',
      'bound': 'END',
      'outcome': 'SOME_OTHER_OUTCOME',
      'nextVersion': '2.0',
    };
    const event = parseEvent(message) as UpdateEndEvent;
    expect(event.outcome).to.equal('SOME_OTHER_OUTCOME');
  });

  test('should parse a valid PERSISTED_DATA event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'PERSISTED_DATA',
      'eventId': 'event4',
      'deviceUptime': '42345',
      'pid': 126,
      'processToken': 'token4',
      'eulaRequired': true,
      'lastChecked': '13408459200000000',  // 2025-11-24T12:00:00Z
      'lastStarted': '13408455600000000',  // 2025-11-24T11:00:00Z
      'registeredApps': [
        {
          'appId': '{app4}',
          'version': '1.0',
          'cohort': 'cohort1',
          'brandCode': 'brand1',
        },
        {
          'appId': '{app5}',
          'version': '2.0',
        },
      ],
    };
    const event = parseEvent(message) as PersistedDataEvent;
    expect(event.eventType).to.equal('PERSISTED_DATA');
    expect(event.bound).to.equal('INSTANT');
    expect(event.eventId).to.equal('event4');
    expect(event.eulaRequired).to.be.true;
    expect(event.lastChecked).to.deep.equal(new Date('2025-11-24T12:00:00Z'));
    expect(event.lastStarted).to.deep.equal(new Date('2025-11-24T11:00:00Z'));
    expect(event.registeredApps).to.deep.equal([
      {
        appId: '{app4}',
        version: '1.0',
        cohort: 'cohort1',
        brandCode: 'brand1',
      },
      {
        appId: '{app5}',
        version: '2.0',
        cohort: undefined,
        brandCode: undefined,
      },
    ]);
    expect(event.errors).to.deep.equal([]);
  });

  test('should parse PERSISTED_DATA with missing optional fields', () => {
    const message: Record<string, unknown> = {
      'eventType': 'PERSISTED_DATA',
      'eventId': 'event4',
      'deviceUptime': '42345',
      'pid': 126,
      'processToken': 'token4',
      'eulaRequired': false,
    };
    const event = parseEvent(message) as PersistedDataEvent;
    expect(event.eventType).to.equal('PERSISTED_DATA');
    expect(event.bound).to.equal('INSTANT');
    expect(event.eventId).to.equal('event4');
    expect(event.eulaRequired).to.be.false;
    expect(event.lastChecked).to.be.undefined;
    expect(event.lastStarted).to.be.undefined;
    expect(event.registeredApps).to.deep.equal([]);
    expect(event.errors).to.deep.equal([]);
  });

  test('should use INSTANT as default bound if not present', () => {
    const message: Record<string, unknown> = {
      'eventType': 'UNINSTALL',
      'eventId': 'event2',
      'deviceUptime': '223456',
      'pid': 124,
      'processToken': 'token2',
    };
    // The parser will throw because UNINSTALL/INSTANT is not supported,
    // but if it didn't, it would parse bound as INSTANT.
    // The error message for unimplemented parser confirms this:
    expect(() => parseEvent(message))
        .to.throw(
            'No parser implemented for UNINSTALL with bound INSTANT',
        );
  });

  test('should parse errors field when present', () => {
    const message: Record<string, unknown> = {
      'eventType': 'INSTALL',
      'eventId': 'event1',
      'deviceUptime': '12345',
      'pid': 123,
      'processToken': 'token1',
      'bound': 'START',
      'appId': '{app1}',
      'errors': [
        {'category': 1, 'code': 2, 'extracode1': 3},
        {'category': 4, 'code': 5, 'extracode1': 6},
      ],
    };
    const event = parseEvent(message) as InstallStartEvent;
    expect(event.errors).to.deep.equal([
      {category: 1, code: 2, extracode1: 3},
      {category: 4, code: 5, extracode1: 6},
    ]);
  });

  test('should parse pid as string', () => {
    const message: Record<string, unknown> = {
      'eventType': 'INSTALL',
      'eventId': 'event1',
      'deviceUptime': '12345',
      'pid': '123',
      'processToken': 'token1',
      'bound': 'START',
      'appId': '{app1}',
    };
    const event = parseEvent(message) as InstallStartEvent;
    expect(event.pid).to.equal(123);
  });

  test('should parse a valid QUALIFY START event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'QUALIFY',
      'eventId': 'event5',
      'deviceUptime': '52345',
      'pid': 127,
      'processToken': 'token5',
      'bound': 'START',
    };
    const event = parseEvent(message) as QualifyStartEvent;
    expect(event.eventType).to.equal('QUALIFY');
    expect(event.bound).to.equal('START');
  });

  test('should parse a valid QUALIFY END event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'QUALIFY',
      'eventId': 'event5',
      'deviceUptime': '523456',
      'pid': 127,
      'processToken': 'token5',
      'bound': 'END',
      'qualified': true,
    };
    const event = parseEvent(message) as QualifyEndEvent;
    expect(event.eventType).to.equal('QUALIFY');
    expect(event.bound).to.equal('END');
    expect(event.qualified).to.be.true;
  });

  test('should parse a valid ACTIVATE START event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'ACTIVATE',
      'eventId': 'event6',
      'deviceUptime': '62345',
      'pid': 128,
      'processToken': 'token6',
      'bound': 'START',
    };
    const event = parseEvent(message) as ActivateStartEvent;
    expect(event.eventType).to.equal('ACTIVATE');
    expect(event.bound).to.equal('START');
  });

  test('should parse a valid ACTIVATE END event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'ACTIVATE',
      'eventId': 'event6',
      'deviceUptime': '623456',
      'pid': 128,
      'processToken': 'token6',
      'bound': 'END',
      'activated': true,
    };
    const event = parseEvent(message) as ActivateEndEvent;
    expect(event.eventType).to.equal('ACTIVATE');
    expect(event.bound).to.equal('END');
    expect(event.activated).to.be.true;
  });

  test('should parse a valid POST_REQUEST START event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'POST_REQUEST',
      'eventId': 'event7',
      'deviceUptime': '72345',
      'pid': 129,
      'processToken': 'token7',
      'bound': 'START',
      'request': 'foo',
    };
    const event = parseEvent(message) as PostRequestStartEvent;
    expect(event.eventType).to.equal('POST_REQUEST');
    expect(event.bound).to.equal('START');
    expect(event.request).to.equal('foo');
  });

  test('should parse a valid POST_REQUEST END event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'POST_REQUEST',
      'eventId': 'event7',
      'deviceUptime': '723456',
      'pid': 129,
      'processToken': 'token7',
      'bound': 'END',
      'response': 'bar',
    };
    const event = parseEvent(message) as PostRequestEndEvent;
    expect(event.eventType).to.equal('POST_REQUEST');
    expect(event.bound).to.equal('END');
    expect(event.response).to.equal('bar');
  });

  test('should parse a valid LOAD_POLICY START event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'LOAD_POLICY',
      'eventId': 'event8',
      'deviceUptime': '82345',
      'pid': 130,
      'processToken': 'token8',
      'bound': 'START',
    };
    const event = parseEvent(message) as LoadPolicyStartEvent;
    expect(event.eventType).to.equal('LOAD_POLICY');
    expect(event.bound).to.equal('START');
  });

  test('should parse a valid LOAD_POLICY END event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'LOAD_POLICY',
      'eventId': 'event8',
      'deviceUptime': '823456',
      'pid': 130,
      'processToken': 'token8',
      'bound': 'END',
      'policySet': {
        'policiesByName': {
          'policy1': {
            'valuesBySource': {'default': 1},
            'prevailingSource': 'default',
          },
        },
        'policiesByAppId': {
          '{app1}': {
            'policy2': {
              'valuesBySource': {'platform': 2},
              'prevailingSource': 'platform',
            },
          },
        },
      },
    };
    const event = parseEvent(message) as LoadPolicyEndEvent;
    expect(event.eventType).to.equal('LOAD_POLICY');
    expect(event.bound).to.equal('END');
    expect(event.policySet).to.deep.equal({
      policiesByName: {
        'policy1': {
          valuesBySource: {'default': 1},
          prevailingSource: 'default',
        },
      },
      policiesByAppId: {
        '{app1}': {
          'policy2': {
            valuesBySource: {'platform': 2},
            prevailingSource: 'platform',
          },
        },
      },
    });
  });

  test('should parse a valid UPDATER_PROCESS START event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'UPDATER_PROCESS',
      'eventId': 'event9',
      'deviceUptime': '92345',
      'pid': 131,
      'processToken': 'token9',
      'bound': 'START',
      'commandLine': 'foo --bar',
      'timestamp': '13408459200000000',  // 2025-11-24T12:00:00Z,
      'updaterVersion': '1.0',
      'scope': 'USER',
      'osPlatform': 'Mac',
      'osVersion': '10.15.7',
      'osArchitecture': 'x86_64',
      'updaterArchitecture': 'x86_64',
      'parentPid': 1,
    };
    const event = parseEvent(message) as UpdaterProcessStartEvent;
    expect(event.eventType).to.equal('UPDATER_PROCESS');
    expect(event.bound).to.equal('START');
    expect(event.commandLine).to.equal('foo --bar');
    expect(event.timestamp).to.deep.equal(new Date('2025-11-24T12:00:00Z'));
    expect(event.updaterVersion).to.equal('1.0');
    expect(event.scope).to.equal('USER');
    expect(event.osPlatform).to.equal('Mac');
    expect(event.osVersion).to.equal('10.15.7');
    expect(event.osArchitecture).to.equal('x86_64');
    expect(event.updaterArchitecture).to.equal('x86_64');
    expect(event.parentPid).to.equal(1);
  });

  test('should parse a valid UPDATER_PROCESS END event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'UPDATER_PROCESS',
      'eventId': 'event9',
      'deviceUptime': '923456',
      'pid': 131,
      'processToken': 'token9',
      'bound': 'END',
      'exitCode': 0,
    };
    const event = parseEvent(message) as UpdaterProcessEndEvent;
    expect(event.eventType).to.equal('UPDATER_PROCESS');
    expect(event.bound).to.equal('END');
    expect(event.exitCode).to.equal(0);
  });

  test('should parse a valid APP_COMMAND START event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'APP_COMMAND',
      'eventId': 'event10',
      'deviceUptime': '102345',
      'pid': 132,
      'processToken': 'token10',
      'bound': 'START',
      'appId': '{app1}',
      'commandLine': 'foo --bar',
    };
    const event = parseEvent(message) as AppCommandStartEvent;
    expect(event.eventType).to.equal('APP_COMMAND');
    expect(event.bound).to.equal('START');
    expect(event.appId).to.equal('{app1}');
    expect(event.commandLine).to.equal('foo --bar');
  });

  test('should parse a valid APP_COMMAND END event', () => {
    const message: Record<string, unknown> = {
      'eventType': 'APP_COMMAND',
      'eventId': 'event10',
      'deviceUptime': '1023456',
      'pid': 132,
      'processToken': 'token10',
      'bound': 'END',
      'exitCode': 0,
      'output': 'foo',
    };
    const event = parseEvent(message) as AppCommandEndEvent;
    expect(event.eventType).to.equal('APP_COMMAND');
    expect(event.bound).to.equal('END');
    expect(event.exitCode).to.equal(0);
    expect(event.output).to.equal('foo');
  });

  suite('error handling', () => {
    const baseInstallStart: Record<string, unknown> = {
      'eventType': 'INSTALL',
      'eventId': 'event1',
      'deviceUptime': '12345',
      'pid': 123,
      'processToken': 'token1',
      'bound': 'START',
      'appId': '{app1}',
    };

    for (const field
             of ['eventType',
                 'eventId',
                 'deviceUptime',
                 'pid',
                 'processToken',
    ]) {
      test(`should throw if required base field '${field}' is missing`, () => {
        const message: Record<string, unknown> = {...baseInstallStart};
        delete message[field];
        expect(() => parseEvent(message))
            .to.throw(
                `Message missing required field '${field}'`,
            );
      });
    }

    test(
        `should throw if required field 'appId' is missing for INSTALL START`,
        () => {
          const message: Record<string, unknown> = {...baseInstallStart};
          delete message['appId'];
          expect(() => parseEvent(message))
              .to.throw(
                  `Message missing required field 'appId'`,
              );
        });

    test(
        `should throw if required field 'version' is missing for INSTALL END`,
        () => {
          const message: Record<string, unknown> = {
            'eventType': 'INSTALL',
            'eventId': 'event1',
            'deviceUptime': '123456',
            'pid': 123,
            'processToken': 'token1',
            'bound': 'END',
          };
          expect(() => parseEvent(message))
              .to.throw(
                  `Message missing required field 'version'`,
              );
        });

    test(
        `should throw if required field 'appId' is missing for UNINSTALL START`,
        () => {
          const message: Record<string, unknown> = {
            'eventType': 'UNINSTALL',
            'eventId': 'event2',
            'deviceUptime': '22345',
            'pid': 124,
            'processToken': 'token2',
            'bound': 'START',
            'version': '1.0',
            'reason': 'UNINSTALLED',
          };
          delete message['appId'];
          expect(() => parseEvent(message))
              .to.throw(
                  `Message missing required field 'appId'`,
              );
        });
    test(
        `should throw if required field 'version' is missing for UNINSTALL START`,
        () => {
          const message: Record<string, unknown> = {
            'eventType': 'UNINSTALL',
            'eventId': 'event2',
            'deviceUptime': '22345',
            'pid': 124,
            'processToken': 'token2',
            'bound': 'START',
            'appId': '{app2}',
            'reason': 'UNINSTALLED',
          };
          delete message['version'];
          expect(() => parseEvent(message))
              .to.throw(
                  `Message missing required field 'version'`,
              );
        });
    test(
        `should throw if required field 'reason' is missing for UNINSTALL START`,
        () => {
          const message: Record<string, unknown> = {
            'eventType': 'UNINSTALL',
            'eventId': 'event2',
            'deviceUptime': '22345',
            'pid': 124,
            'processToken': 'token2',
            'bound': 'START',
            'appId': '{app2}',
            'version': '1.0',
          };
          delete message['reason'];
          expect(() => parseEvent(message))
              .to.throw(
                  `Message missing required field 'reason'`,
              );
        });

    test(
        `should throw if required field 'eulaRequired' is missing for PERSISTED_DATA`,
        () => {
          const message: Record<string, unknown> = {
            'eventType': 'PERSISTED_DATA',
            'eventId': 'event4',
            'deviceUptime': '42345',
            'pid': 126,
            'processToken': 'token4',
          };
          expect(() => parseEvent(message))
              .to.throw(
                  `Message missing required field 'eulaRequired'`,
              );
        });

    test('should throw for unknown eventType', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'eventType': 'UNKNOWN',
      };
      expect(() => parseEvent(message))
          .to.throw(
              'Message contains unknown eventType: UNKNOWN',
          );
    });

    test('should throw for unknown bound', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'bound': 'UNKNOWN',
      };
      expect(() => parseEvent(message))
          .to.throw(
              'Message contains unknown bound: UNKNOWN',
          );
    });

    test('should throw for invalid pid type', () => {
      const message: Record<string, unknown> = {...baseInstallStart, 'pid': {}};
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'pid' with unexpected type 'object', expected 'number' or 'string'`,
          );
    });

    test('should throw for non-finite pid', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'pid': Infinity,
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'pid' with a numeric value that is not an integer.`,
          );
    });

    test('should throw for string pid that is not a number', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'pid': 'abc',
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'pid' with a numeric value that is not an integer.`,
          );
    });

    test('should throw for decimal pid', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'pid': 123.45,
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'pid' with a numeric value that is not an integer.`,
          );
    });

    test('should throw for string pid that is a decimal number', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'pid': '123.45',
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'pid' with a numeric value that is not an integer.`,
          );
    });

    test('should throw for invalid eventId type', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'eventId': 123,
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field eventId with unexpected type 'number', expected 'string'`,
          );
    });

    test('should throw if errors is not an array', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'errors': {},
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'errors' of unexpected non-array type 'object'.`,
          );
    });

    test('should throw if errors contains non-object', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'errors': [123],
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'errors' containing an element of unexpected type 'number', expected 'object'.`,
          );
    });

    test('should throw if errors contains an array', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'errors': [[]],
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'errors' of unexpected array type.`,
          );
    });

    test('should throw if error item is missing category', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'errors': [{'code': 2, 'extracode1': 3}],
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message missing required field 'category'`,
          );
    });

    test('should parse numeric error fields from strings', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'errors': [{'category': '1', 'code': '2', 'extracode1': '3'}],
      };
      const event = parseEvent(message);
      expect(event.errors).to.deep.equal([
        {category: 1, code: 2, extracode1: 3},
      ]);
    });

    test('should throw if error item has non-numeric type for category', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'errors': [{'category': {}, 'code': 2, 'extracode1': 3}],
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'category' with unexpected type 'object', expected 'number' or 'string'`,
          );
    });

    test('should throw if error item has decimal type for category', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'errors': [{'category': 1.23, 'code': 2, 'extracode1': 3}],
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'category' with a numeric value that is not an integer.`,
          );
    });

    test('should throw for unimplemented event type', () => {
      const message: Record<string, unknown> = {
        ...baseInstallStart,
        'eventType': 'FOOBAR',
      };
      expect(() => parseEvent(message))
          .to.throw(
              'Message contains unknown eventType: FOOBAR',
          );
    });

    test('should throw if date field has invalid date string', () => {
      const message: Record<string, unknown> = {
        'eventType': 'PERSISTED_DATA',
        'eventId': 'event4',
        'deviceUptime': '42345',
        'pid': 126,
        'processToken': 'token4',
        'eulaRequired': true,
        'lastChecked': 'not-a-number',
      };
      expect(() => parseEvent(message))
          .to.throw(
              `Message has field 'lastChecked' with unparsable datetime value 'not-a-number'`,
          );
    });

    suite('LOAD_POLICY END message validation', () => {
      const baseLoadPolicyEndMessage: Record<string, unknown> = {
        'eventType': 'LOAD_POLICY',
        'eventId': 'event8',
        'deviceUptime': '823456',
        'pid': 130,
        'processToken': 'token8',
        'bound': 'END',
      };

      test('should throw if policySet is not an object', () => {
        const message: Record<string, unknown> = {
          ...baseLoadPolicyEndMessage,
          'policySet': 123,
        };
        expect(() => parseEvent(message))
            .to.throw(
                `Message has field policySet with unexpected type 'number', expected 'object'`,
            );
      });

      test(`should throw if 'policiesByName' is missing`, () => {
        const message: Record<string, unknown> = {
          ...baseLoadPolicyEndMessage,
          'policySet': {'policiesByAppId': {}},
        };
        expect(() => parseEvent(message))
            .to.throw(
                `Message missing required field 'policiesByName'`,
            );
      });

      test(`should throw if 'policiesByAppId' is missing`, () => {
        const message: Record<string, unknown> = {
          ...baseLoadPolicyEndMessage,
          'policySet': {'policiesByName': {}},
        };
        expect(() => parseEvent(message))
            .to.throw(
                `Message missing required field 'policiesByAppId'`,
            );
      });

      test(`should throw if a policy in 'policiesByName' is not an object`, () => {
        const message: Record<string, unknown> = {
          ...baseLoadPolicyEndMessage,
          'policySet': {
            'policiesByName': {'policy1': 123},
            'policiesByAppId': {},
          },
        };
        expect(() => parseEvent(message))
            .to.throw(
                `Message has field policy1 with unexpected type 'number', expected 'object'`,
            );
      });

      test(
          `should throw if a policy in 'policiesByName' is missing 'valuesBySource'`,
          () => {
            const message: Record<string, unknown> = {
              ...baseLoadPolicyEndMessage,
              'policySet': {
                'policiesByName': {'policy1': {'prevailingSource': 'default'}},
                'policiesByAppId': {},
              },
            };
            expect(() => parseEvent(message))
                .to.throw(
                    `Message missing required field 'valuesBySource'`,
                );
          });

      test(
          `should throw if a policy in 'policiesByName' is missing 'prevailingSource'`,
          () => {
            const message: Record<string, unknown> = {
              ...baseLoadPolicyEndMessage,
              'policySet': {
                'policiesByName':
                    {'policy1': {'valuesBySource': {'default': 1}}},
                'policiesByAppId': {},
              },
            };
            expect(() => parseEvent(message))
                .to.throw(
                    `Message missing required field 'prevailingSource'`,
                );
          });

      test(
          `should throw if an app policy in 'policiesByAppId' is not an object`,
          () => {
            const message: Record<string, unknown> = {
              ...baseLoadPolicyEndMessage,
              'policySet': {
                'policiesByName': {},
                'policiesByAppId': {'{app1}': 123},
              },
            };
            expect(() => parseEvent(message))
                .to.throw(
                    `Message has field {app1} with unexpected type 'number', expected 'object'`,
                );
          });

      test(`should throw if a policy in 'policiesByAppId' is not an object`, () => {
        const message: Record<string, unknown> = {
          ...baseLoadPolicyEndMessage,
          'policySet': {
            'policiesByName': {},
            'policiesByAppId': {'{app1}': {'policy2': 123}},
          },
        };
        expect(() => parseEvent(message))
            .to.throw(
                `Message has field policy2 with unexpected type 'number', expected 'object'`,
            );
      });

      test(
          `should throw if a policy in 'policiesByAppId' is missing 'valuesBySource'`,
          () => {
            const message: Record<string, unknown> = {
              ...baseLoadPolicyEndMessage,
              'policySet': {
                'policiesByName': {},
                'policiesByAppId': {
                  '{app1}': {'policy2': {'prevailingSource': 'platform'}},
                },
              },
            };
            expect(() => parseEvent(message))
                .to.throw(
                    `Message missing required field 'valuesBySource'`,
                );
          });

      test(
          `should throw if a policy in 'policiesByAppId' is missing 'prevailingSource'`,
          () => {
            const message: Record<string, unknown> = {
              ...baseLoadPolicyEndMessage,
              'policySet': {
                'policiesByName': {},
                'policiesByAppId': {
                  '{app1}': {'policy2': {'valuesBySource': {'platform': 2}}},
                },
              },
            };
            expect(() => parseEvent(message))
                .to.throw(
                    `Message missing required field 'prevailingSource'`,
                );
          });
    });
  });
});

suite('event processor', () => {
  const EVENT1_INSTALL_START: InstallStartEvent = {
    eventType: 'INSTALL',
    eventId: '1',
    pid: 100,
    processToken: 'abc',
    deviceUptime: 1000,
    bound: 'START',
    errors: [],
    appId: 'app1',
  };
  const EVENT1_INSTALL_START_DUP: InstallStartEvent = {
    eventType: 'INSTALL',
    eventId: '1',
    pid: 100,
    processToken: 'abc',
    deviceUptime: 1001,
    bound: 'START',
    errors: [],
    appId: 'app1',
  };
  const EVENT1_INSTALL_END: InstallEndEvent = {
    eventType: 'INSTALL',
    eventId: '1',
    pid: 100,
    processToken: 'abc',
    deviceUptime: 1002,
    bound: 'END',
    errors: [],
    version: '1.0',
  };
  const EVENT2_UNINSTALL_START: UninstallStartEvent = {
    eventType: 'UNINSTALL',
    eventId: '2',
    pid: 100,
    processToken: 'abc',
    deviceUptime: 1003,
    bound: 'START',
    errors: [],
    appId: 'app2',
    version: '1.0',
    reason: 'test',
  };
  const EVENT2_UNINSTALL_END: UninstallEndEvent = {
    eventType: 'UNINSTALL',
    eventId: '2',
    pid: 100,
    processToken: 'abc',
    deviceUptime: 1004,
    bound: 'END',
    errors: [],
  };
  const EVENT3_UPDATE_START: UpdateStartEvent = {
    eventType: 'UPDATE',
    eventId: '3',
    pid: 101,
    processToken: 'def',
    deviceUptime: 1005,
    bound: 'START',
    errors: [],
    appId: 'app3',
    priority: 'BACKGROUND',
  };
  const EVENT4_PERSISTED_DATA: PersistedDataEvent = {
    eventType: 'PERSISTED_DATA',
    eventId: '4',
    pid: 102,
    processToken: 'ghi',
    deviceUptime: 1006,
    bound: 'INSTANT',
    errors: [],
    eulaRequired: false,
    registeredApps: [],
    lastChecked: undefined,
    lastStarted: undefined,
  };

  suite('deduplicateEvents', () => {
    test('should return an empty array when given an empty array', () => {
      expect(deduplicateEvents([])).to.deep.equal([]);
    });

    test('should not remove events that are not duplicates', () => {
      const events: HistoryEvent[] = [
        EVENT1_INSTALL_START,
        EVENT1_INSTALL_END,
        EVENT2_UNINSTALL_START,
      ];
      expect(deduplicateEvents(events)).to.deep.equal(events);
    });

    test('should remove duplicate events', () => {
      const events: HistoryEvent[] = [
        EVENT1_INSTALL_START,
        EVENT1_INSTALL_START_DUP,
        EVENT1_INSTALL_END,
      ];
      expect(deduplicateEvents(events)).to.deep.equal([
        EVENT1_INSTALL_START,
        EVENT1_INSTALL_END,
      ]);
    });

    test('should handle multiple duplicate events', () => {
      const events: HistoryEvent[] = [
        EVENT1_INSTALL_START,
        EVENT1_INSTALL_START_DUP,
        EVENT1_INSTALL_START_DUP,
        EVENT1_INSTALL_END,
      ];
      expect(deduplicateEvents(events)).to.deep.equal([
        EVENT1_INSTALL_START,
        EVENT1_INSTALL_END,
      ]);
    });
  });

  suite('mergeEvents', () => {
    test('should return empty arrays when given an empty array', () => {
      expect(mergeEvents([])).to.deep.equal({paired: [], unpaired: []});
    });

    test('should return INSTANT events as unpaired', () => {
      expect(mergeEvents([EVENT4_PERSISTED_DATA])).to.deep.equal({
        paired: [],
        unpaired: [EVENT4_PERSISTED_DATA],
      });
    });

    test('should pair START and END events with the same key', () => {
      expect(mergeEvents([
        EVENT1_INSTALL_START,
        EVENT1_INSTALL_END,
      ])).to.deep.equal({
        paired: [
          {
            eventType: 'INSTALL',
            startEvent: EVENT1_INSTALL_START,
            endEvent: EVENT1_INSTALL_END,
          },
        ],
        unpaired: [],
      });
    });

    test('should handle multiple pairs', () => {
      expect(
          mergeEvents([
            EVENT1_INSTALL_START,
            EVENT1_INSTALL_END,
            EVENT2_UNINSTALL_START,
            EVENT2_UNINSTALL_END,
          ]),
          )
          .to.deep.equal({
            paired: [
              {
                eventType: 'INSTALL',
                startEvent: EVENT1_INSTALL_START,
                endEvent: EVENT1_INSTALL_END,
              },
              {
                eventType: 'UNINSTALL',
                startEvent: EVENT2_UNINSTALL_START,
                endEvent: EVENT2_UNINSTALL_END,
              },
            ],
            unpaired: [],
          });
    });

    test(
        'should return START event as unpaired if no matching END event',
        () => {
          expect(mergeEvents([EVENT1_INSTALL_START])).to.deep.equal({
            paired: [],
            unpaired: [EVENT1_INSTALL_START],
          });
        });

    test(
        'should return END event as unpaired if no matching START event',
        () => {
          expect(mergeEvents([EVENT1_INSTALL_END])).to.deep.equal({
            paired: [],
            unpaired: [EVENT1_INSTALL_END],
          });
        });

    test('should pair one START and END, leaving extra END as unpaired', () => {
      expect(
          mergeEvents([
            EVENT1_INSTALL_START,
            EVENT1_INSTALL_END,
            EVENT1_INSTALL_END,
          ]),
          )
          .to.deep.equal({
            paired: [
              {
                eventType: 'INSTALL',
                startEvent: EVENT1_INSTALL_START,
                endEvent: EVENT1_INSTALL_END,
              },
            ],
            unpaired: [EVENT1_INSTALL_END],
          });
    });

    test(
        'should pair one START and END, leaving extra START as unpaired',
        () => {
          expect(
              mergeEvents([
                EVENT1_INSTALL_START,
                EVENT1_INSTALL_START,
                EVENT1_INSTALL_END,
              ]),
              )
              .to.deep.equal({
                paired: [
                  {
                    eventType: 'INSTALL',
                    startEvent: EVENT1_INSTALL_START,
                    endEvent: EVENT1_INSTALL_END,
                  },
                ],
                unpaired: [EVENT1_INSTALL_START],
              });
        });

    test(
        'should handle a mixture of paired, unpaired, and instant events',
        () => {
          const result = mergeEvents([
            EVENT1_INSTALL_START,
            EVENT1_INSTALL_END,
            EVENT2_UNINSTALL_END,
            EVENT3_UPDATE_START,
            EVENT4_PERSISTED_DATA,
          ]);
          expect(result.paired).to.deep.equal([
            {
              eventType: 'INSTALL',
              startEvent: EVENT1_INSTALL_START,
              endEvent: EVENT1_INSTALL_END,
            },
          ]);
          expect(result.unpaired).to.have.deep.members([
            EVENT4_PERSISTED_DATA,
            EVENT2_UNINSTALL_END,
            EVENT3_UPDATE_START,
          ]);
        });
  });
});

suite('UpdaterProcessMap', () => {
  const PROCESS1_START: UpdaterProcessStartEvent = {
    eventType: 'UPDATER_PROCESS',
    eventId: 'p1',
    pid: 100,
    processToken: 'abc',
    deviceUptime: 1000000,
    bound: 'START',
    errors: [],
    timestamp: new Date('2025-11-24T12:00:00Z'),
  };
  const PROCESS1_END: UpdaterProcessEndEvent = {
    eventType: 'UPDATER_PROCESS',
    eventId: 'p1',
    pid: 100,
    processToken: 'abc',
    deviceUptime: 2000000,
    bound: 'END',
    errors: [],
    exitCode: 0,
  };
  const PROCESS1_MERGED: MergedUpdaterProcessEvent = {
    eventType: 'UPDATER_PROCESS',
    startEvent: PROCESS1_START,
    endEvent: PROCESS1_END,
  };
  const PROCESS2_START: UpdaterProcessStartEvent = {
    eventType: 'UPDATER_PROCESS',
    eventId: 'p2',
    pid: 200,
    processToken: 'def',
    deviceUptime: 1500000,
    bound: 'START',
    errors: [],
    timestamp: new Date('2025-11-24T12:00:00.500Z'),
  };
  const PROCESS2_END: UpdaterProcessEndEvent = {
    eventType: 'UPDATER_PROCESS',
    eventId: 'p2',
    pid: 200,
    processToken: 'def',
    deviceUptime: 2500000,
    bound: 'END',
    errors: [],
    exitCode: 1,
  };
  const PROCESS2_MERGED: MergedUpdaterProcessEvent = {
    eventType: 'UPDATER_PROCESS',
    startEvent: PROCESS2_START,
    endEvent: PROCESS2_END,
  };
  const INSTALL1_START: InstallStartEvent = {
    eventType: 'INSTALL',
    eventId: 'i1',
    pid: 100,
    processToken: 'abc',
    deviceUptime: 1100000,
    bound: 'START',
    errors: [],
    appId: 'app1',
  };
  const INSTALL1_END: InstallEndEvent = {
    eventType: 'INSTALL',
    eventId: 'i1',
    pid: 100,
    processToken: 'abc',
    deviceUptime: 1200000,
    bound: 'END',
    errors: [],
    version: '1.0',
  };
  const INSTALL1_MERGED: MergedInstallEvent = {
    eventType: 'INSTALL',
    startEvent: INSTALL1_START,
    endEvent: INSTALL1_END,
  };
  const INSTALL2_START: InstallStartEvent = {
    eventType: 'INSTALL',
    eventId: 'i2',
    pid: 300,
    processToken: 'ghi',
    deviceUptime: 1300000,
    bound: 'START',
    errors: [],
    appId: 'app2',
  };
  const INSTALL2_END: InstallEndEvent = {
    eventType: 'INSTALL',
    eventId: 'i2',
    pid: 300,
    processToken: 'ghi',
    deviceUptime: 1400000,
    bound: 'END',
    errors: [],
    version: '1.0',
  };
  const INSTALL2_MERGED: MergedInstallEvent = {
    eventType: 'INSTALL',
    startEvent: INSTALL2_START,
    endEvent: INSTALL2_END,
  };

  test('should be empty when initialized with no events', () => {
    const map = new UpdaterProcessMap([]);
    expect(map.getUpdaterProcessForEvent(INSTALL1_MERGED)).to.be.undefined;
  });

  test(
      'should be empty when initialized with no UPDATER_PROCESS events', () => {
        const map = new UpdaterProcessMap([INSTALL1_MERGED]);
        expect(map.getUpdaterProcessForEvent(INSTALL1_MERGED)).to.be.undefined;
      });

  test(
      'should retrieve the updater process for an event in that process',
      () => {
        const map = new UpdaterProcessMap([PROCESS1_MERGED, INSTALL1_MERGED]);
        expect(map.getUpdaterProcessForEvent(INSTALL1_MERGED))
            .to.equal(
                PROCESS1_MERGED,
            );
      });

  test(
      'should retrieve the correct updater process when multiple are present',
      () => {
        const map = new UpdaterProcessMap([
          PROCESS1_MERGED,
          PROCESS2_MERGED,
          INSTALL1_MERGED,
        ]);
        expect(map.getUpdaterProcessForEvent(INSTALL1_MERGED))
            .to.equal(
                PROCESS1_MERGED,
            );
      });

  test(
      'should return undefined for an event whose process is not in map',
      () => {
        const map = new UpdaterProcessMap([PROCESS1_MERGED, INSTALL2_MERGED]);
        expect(map.getUpdaterProcessForEvent(INSTALL2_MERGED)).to.be.undefined;
      });

  test('should handle multiple updater processes and events', () => {
    const INSTALL_IN_PROCESS2_START: InstallStartEvent = {
      eventType: 'INSTALL',
      eventId: 'i3',
      pid: 200,
      processToken: 'def',
      deviceUptime: 1600,
      bound: 'START',
      errors: [],
      appId: 'app3',
    };
    const INSTALL_IN_PROCESS2_END: InstallEndEvent = {
      eventType: 'INSTALL',
      eventId: 'i3',
      pid: 200,
      processToken: 'def',
      deviceUptime: 1700,
      bound: 'END',
      errors: [],
      version: '1.0',
    };
    const INSTALL_IN_PROCESS2_MERGED: MergedInstallEvent = {
      eventType: 'INSTALL',
      startEvent: INSTALL_IN_PROCESS2_START,
      endEvent: INSTALL_IN_PROCESS2_END,
    };
    const map = new UpdaterProcessMap([
      PROCESS1_MERGED,
      PROCESS2_MERGED,
      INSTALL1_MERGED,
      INSTALL_IN_PROCESS2_MERGED,
    ]);
    expect(map.getUpdaterProcessForEvent(INSTALL1_MERGED))
        .to.equal(
            PROCESS1_MERGED,
        );
    expect(map.getUpdaterProcessForEvent(INSTALL_IN_PROCESS2_MERGED))
        .to.equal(
            PROCESS2_MERGED,
        );
  });

  test('an UPDATER_PROCESS event should retrieve itself', () => {
    const map = new UpdaterProcessMap([PROCESS1_MERGED, PROCESS2_MERGED]);
    expect(map.getUpdaterProcessForEvent(PROCESS1_MERGED))
        .to.equal(
            PROCESS1_MERGED,
        );
    expect(map.getUpdaterProcessForEvent(PROCESS2_MERGED))
        .to.equal(
            PROCESS2_MERGED,
        );
  });

  test('should retrieve updater process for unmerged events', () => {
    const map = new UpdaterProcessMap([
      PROCESS1_MERGED,
      PROCESS2_MERGED,
      INSTALL1_MERGED,
    ]);
    expect(map.getUpdaterProcessForEvent(INSTALL1_START))
        .to.equal(PROCESS1_MERGED);
    expect(map.getUpdaterProcessForEvent(INSTALL1_END))
        .to.equal(PROCESS1_MERGED);
    expect(map.getUpdaterProcessForEvent(PROCESS1_START))
        .to.equal(PROCESS1_MERGED);
    expect(map.getUpdaterProcessForEvent(PROCESS1_END))
        .to.equal(PROCESS1_MERGED);
    expect(map.getUpdaterProcessForEvent(INSTALL2_START)).to.be.undefined;
  });

  suite('eventDate', () => {
    test('should return undefined if event has no updater process', () => {
      const map = new UpdaterProcessMap([PROCESS1_MERGED]);
      expect(map.eventDate(INSTALL2_MERGED)).to.be.undefined;
    });

    test('should return undefined if updater process has no timestamp', () => {
      const PROCESS_NO_TIMESTAMP_START: UpdaterProcessStartEvent = {
        eventType: 'UPDATER_PROCESS',
        eventId: 'p0',
        pid: 99,
        processToken: 'xyz',
        deviceUptime: 1000000,
        bound: 'START',
        errors: [],
      };
      const PROCESS_NO_TIMESTAMP_END: UpdaterProcessEndEvent = {
        eventType: 'UPDATER_PROCESS',
        eventId: 'p0',
        pid: 99,
        processToken: 'xyz',
        deviceUptime: 2000000,
        bound: 'END',
        errors: [],
        exitCode: 0,
      };
      const PROCESS_NO_TIMESTAMP_MERGED: MergedUpdaterProcessEvent = {
        eventType: 'UPDATER_PROCESS',
        startEvent: PROCESS_NO_TIMESTAMP_START,
        endEvent: PROCESS_NO_TIMESTAMP_END,
      };
      const INSTALL_NO_TIMESTAMP_START: InstallStartEvent = {
        eventType: 'INSTALL',
        eventId: 'i0',
        pid: 99,
        processToken: 'xyz',
        deviceUptime: 1100000,
        bound: 'START',
        errors: [],
        appId: 'app0',
      };
      const map = new UpdaterProcessMap([PROCESS_NO_TIMESTAMP_MERGED]);
      expect(map.eventDate(INSTALL_NO_TIMESTAMP_START)).to.be.undefined;
    });

    test('should calculate event date for an event in a process', () => {
      const map = new UpdaterProcessMap([PROCESS1_MERGED]);
      expect(map.eventDate(INSTALL1_START))
          .to.deep.equal(
              new Date('2025-11-24T12:00:00.100Z'),
          );
      expect(map.eventDate(INSTALL1_END))
          .to.deep.equal(
              new Date('2025-11-24T12:00:00.200Z'),
          );
      expect(map.eventDate(INSTALL1_MERGED))
          .to.deep.equal(
              new Date('2025-11-24T12:00:00.100Z'),
          );
    });
  });

  suite('sortEventsByDate', () => {
    test('should sort events by date', () => {
      const map = new UpdaterProcessMap([PROCESS1_MERGED, PROCESS2_MERGED]);
      const result = map.sortEventsByDate(
          [INSTALL1_END, INSTALL1_START],
          [PROCESS2_MERGED, PROCESS1_MERGED],
      );
      expect(result.sortedEventsWithDates).to.deep.equal([
        PROCESS2_MERGED,
        INSTALL1_END,
        INSTALL1_START,
        PROCESS1_MERGED,
      ]);
      expect(result.unsortedEventsWithoutDates).to.deep.equal([]);
    });

    test('should handle events without dates', () => {
      const map = new UpdaterProcessMap([PROCESS1_MERGED]);
      const result = map.sortEventsByDate([INSTALL2_START], [INSTALL1_MERGED]);
      expect(result.sortedEventsWithDates).to.deep.equal([INSTALL1_MERGED]);
      expect(result.unsortedEventsWithoutDates).to.deep.equal([INSTALL2_START]);
    });
  });
});
