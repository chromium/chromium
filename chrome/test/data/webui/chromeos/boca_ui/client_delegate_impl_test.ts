// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UrlType} from 'chrome-untrusted://boca-app/app/boca_app.js';
import {ClientDelegateFactory, getNetworkInfoMojomToUI, getSessionConfigMojomToUI, getStudentActivityMojomToUI} from 'chrome-untrusted://boca-app/app/client_delegate.js';
import type {AddStudentsError, Assignment, BocaValidPref, CaptionConfig, Config, Course, CreateSessionError, EndViewScreenSessionError, Identity, OnTaskConfig, Permission, PermissionSetting, RemoveStudentError, RenotifyStudentError, SessionResult, SetViewScreenSessionActiveError, UpdateSessionError, ViewStudentScreenError, Window} from 'chrome-untrusted://boca-app/mojom/boca.mojom-webui.js';
import {PageHandlerRemote, SubmitAccessCodeError, UrlType as UrlTypeMojo} from 'chrome-untrusted://boca-app/mojom/boca.mojom-webui.js';
import type {TimeDelta} from 'chrome-untrusted://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {Value} from 'chrome-untrusted://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';
import {assertDeepEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

class MockRemoteHandler extends PageHandlerRemote {
  urlTypeMojo: UrlTypeMojo|null = null;
  override getWindowsTabsList(): Promise<{windowList: Window[]}> {
    return Promise.resolve({
      windowList: [
        {
          name: 'window1',
          tabList: [
            {
              id: 1,
              title: 'title1',
              url: 'http://foo1',
              favicon: 'dataurl1',
              urlType: null,
            },
            {
              id: null,
              title: 'title2',
              url: 'http://foo2',
              favicon: 'dataurl2',
              urlType: null,
            },
            {
              id: 2,
              title: 'Special Url',
              url: 'http://special',
              favicon: 'dataurl3',
              urlType: this.urlTypeMojo,
            },
          ],
        },
        {
          tabList: [{
            id: null,
            title: 'title3',
            url: 'http://foo3',
            favicon: 'dataurl3',
            urlType: null,
          }],
        },
      ] as Window[],
    });
  }
  override listCourses(): Promise<{courses: Course[]}> {
    return Promise.resolve({
      courses: [
        {id: '1', name: 'course1', section: 'period1'},
        {id: '2', name: 'course2', section: ''},
      ],
    });
  }
  override listStudents(id: string): Promise<{students: Identity[]}> {
    // Dummy action get around with unused variable check.
    id;
    return Promise.resolve({
      students: [
        {id: '1', name: 'cat', email: 'email1', photoUrl: 'cdn1'},
        {id: '2', name: 'dog', email: 'email2', photoUrl: 'cdn2'},
      ],
    });
  }
  override listAssignments(id: string): Promise<{assignments: Assignment[]}> {
    // Dummy action get around with unused variable check.
    id;
    return Promise.resolve({
      assignments: [
        {
          title: 'assignment-title1',
          url: 'url1',
          lastUpdateTime: new Date(1000000),
          materials: [
            {title: 'material-title-1', type: 0},
            {title: 'material-title-2', type: 1},
          ],
          type: 0,
        },
        {
          title: 'assignment-title2',
          url: 'url2',
          lastUpdateTime: new Date(2000000),
          materials: [
            {title: 'material-title-3', type: 2},
            {title: 'material-title-4', type: 3},
          ],
          type: 1,
        },
      ],
    });
  }

  override createSession(config: Config):
      Promise<{error: CreateSessionError | null}> {
    assertDeepEquals(
        {
          sessionDuration: {
            // BigInt serialized as string.
            microseconds: 7200000000n,
          },
          students: [
            {
              id: '1',
              name: 'cat',
              email: 'cat@gmail.com',
              photoUrl: 'cdn1',
            },
            {
              id: '2',
              name: 'dog',
              email: 'dog@gmail.com',
              photoUrl: 'cdn2',
            },
          ],
          studentsJoinViaCode: [],
          teacher: null,
          accessCode: null,
          sessionStartTime: null,
          onTaskConfig: {
            isLocked: true,
            isPaused: true,
            tabs: [
              {
                tab: {
                  id: null,
                  url: 'http://google.com/',
                  title: 'google',
                  favicon: 'data/image',
                  urlType: null,
                },
                navigationType: 0,
              },
              {
                tab: {
                  id: null,
                  url: 'http://youtube.com/',
                  title: 'youtube',
                  favicon: 'data/image',
                  urlType: null,
                },
                navigationType: 1,
              },
              {
                tab: {
                  id: null,
                  url: 'http://specialurl.com/',
                  title: 'special url',
                  favicon: 'data/image',
                  urlType: this.urlTypeMojo,
                },
                navigationType: 0,
              },
            ],
          },
          captionConfig: {
            sessionCaptionEnabled: true,
            localCaptionEnabled: true,
            sessionTranslationEnabled: true,
          },
        },
        config);
    return Promise.resolve({error: null});
  }

  override getSession(): Promise<{result: SessionResult}> {
    return Promise.resolve({
      result: {
        session: {
          config: {
            sessionDuration: {
              microseconds: 120000000n,
            },
            sessionStartTime: new Date(1000000),
            teacher: {
              id: '0',
              name: 'teacher',
              email: 'teacher@gmail.com',
              photoUrl: 'cdn0',
            },
            accessCode: 'testCode',
            students: [
              {
                id: '1',
                name: 'cat',
                email: 'cat@gmail.com',
                photoUrl: 'cdn1',
              },
              {
                id: '2',
                name: 'dog',
                email: 'dog@gmail.com',
                photoUrl: 'cdn2',
              },
            ],
            studentsJoinViaCode: [{
              id: '3',
              name: 'cat1',
              email: 'cat1@gmail.com',
              photoUrl: 'cdn3',
            }],
            onTaskConfig: {
              isLocked: true,
              isPaused: true,
              tabs: [
                {
                  tab: {
                    id: 1,
                    url: 'http://google.com/',
                    title: 'google',
                    favicon: 'data/image',
                    urlType: null,
                  },
                  navigationType: 0,
                },
                {
                  tab: {
                    id: null,
                    url: 'http://youtube.com/',
                    title: 'youtube',
                    favicon: 'data/image',
                    urlType: null,
                  },
                  navigationType: 1,
                },
                {
                  tab: {
                    id: null,
                    url: 'http://specialurl.com/',
                    title: 'special url',
                    favicon: 'data/image',
                    urlType: this.urlTypeMojo,
                  },
                  navigationType: 0,
                },
              ],
            },
            captionConfig: {
              sessionCaptionEnabled: true,
              localCaptionEnabled: true,
              sessionTranslationEnabled: true,
            },
          },
          activities: [],
        },
      },
    });
  }

  override updateOnTaskConfig(config: OnTaskConfig):
      Promise<{error: UpdateSessionError | null}> {
    assertDeepEquals(
        {
          isLocked: true,
          isPaused: true,
          tabs: [
            {
              tab: {
                id: null,
                url: 'http://google.com/',
                title: 'google',
                favicon: 'data/image',
                urlType: null,
              },
              navigationType: 0,
            },
            {
              tab: {
                id: null,
                url: 'http://youtube.com/',
                title: 'youtube',
                favicon: 'data/image',
                urlType: null,
              },
              navigationType: 1,
            },
            {
              tab: {
                id: null,
                url: 'http://specialurl.com/',
                title: 'special url',
                favicon: 'data/image',
                urlType: this.urlTypeMojo,
              },
              navigationType: 0,
            },
          ],
        },
        config);
    return Promise.resolve({error: null});
  }

  override updateCaptionConfig(config: CaptionConfig):
      Promise<{error: UpdateSessionError | null}> {
    assertDeepEquals(
        {
          sessionCaptionEnabled: true,
          sessionTranslationEnabled: true,
          localCaptionEnabled: true,
        },
        config);
    return Promise.resolve({error: null});
  }

  override endSession(): Promise<{error: UpdateSessionError | null}> {
    return Promise.resolve({error: null});
  }

  override extendSessionDuration(duration: TimeDelta):
      Promise<{error: UpdateSessionError | null}> {
    assertDeepEquals({microseconds: 900000000n}, duration);
    return Promise.resolve({error: null});
  }

  override removeStudent(id: string):
      Promise<{error: RemoveStudentError | null}> {
    id;
    return Promise.resolve({error: null});
  }

  override addStudents(students: Identity[]):
      Promise<{error: AddStudentsError | null}> {
    assertDeepEquals(
        [
          {
            id: '1',
            name: 'cat',
            email: 'cat@gmail.com',
            photoUrl: 'cdn1',
          },
          {
            id: '2',
            name: 'dog',
            email: 'dog@gmail.com',
            photoUrl: 'cdn2',
          },
        ],
        students);
    return Promise.resolve({error: null});
  }

  override setFloatMode(isFloatMode: boolean): Promise<{success: boolean}> {
    isFloatMode;
    return Promise.resolve({success: true});
  }
  override submitAccessCode(code: string):
      Promise<{error: SubmitAccessCodeError | null}> {
    if (code === 'valid') {
      return Promise.resolve({error: null});
    } else {
      return Promise.resolve({error: SubmitAccessCodeError.kInvalid});
    }
  }
  override viewStudentScreen(id: string):
      Promise<{error: ViewStudentScreenError | null}> {
    id;
    return Promise.resolve({error: null});
  }
  override endViewScreenSession(id: string):
      Promise<{error: EndViewScreenSessionError | null}> {
    id;
    return Promise.resolve({error: null});
  }
  override setViewScreenSessionActive(id: string):
      Promise<{error: SetViewScreenSessionActiveError | null}> {
    id;
    return Promise.resolve({error: null});
  }
  override getUserPref(pref: BocaValidPref): Promise<{value: Value}> {
    pref;
    return Promise.resolve({value: {stringValue: 'value'}});
  }
  override setUserPref(pref: BocaValidPref, value: Value) {
    pref;
    value;
    return Promise.resolve();
  }
  override setSitePermission(
      url: string, permission: Permission,
      setting: PermissionSetting): Promise<{success: boolean}> {
    url;
    permission;
    setting;
    return Promise.resolve({success: true});
  }
  override openFeedbackDialog() {
    return Promise.resolve();
  }

  override refreshWorkbook() {
    return Promise.resolve();
  }

  override renotifyStudent(id: string):
      Promise<{error: RenotifyStudentError | null}> {
    id;
    return Promise.resolve({error: null});
  }

  override startSpotlight(crdConnectionCode: string) {
    crdConnectionCode;
    return Promise.resolve();
  }
}

suite('ClientDelegateTest', function() {
  let clientDelegateImpl: ClientDelegateFactory;
  let remoteHandler: MockRemoteHandler;
  const urlTypeTestCases = [
    {
      name: 'Gemini Regular',
      urlTypeMojo: UrlTypeMojo.kGeminiRegular,
      urlTypeUi: UrlType.GEMINI_REGULAR,
    },
    {
      name: 'Gemini Guided Learning',
      urlTypeMojo: UrlTypeMojo.kGeminiGuidedLearning,
      urlTypeUi: UrlType.GEMINI_GUIDED_LEARNING,
    },
  ];

  setup(function() {
    remoteHandler = new MockRemoteHandler();
    clientDelegateImpl = new ClientDelegateFactory(remoteHandler);
  });

  urlTypeTestCases.forEach((testCase) => {
    test(
        `client delegate should properly translate mojom layer data for ` +
            `windows list with urlType: ${testCase.name}`,
        async () => {
          remoteHandler.urlTypeMojo = testCase.urlTypeMojo;
          const result =
              await clientDelegateImpl.getInstance().getWindowsTabsList();
          assertDeepEquals(
              [
                {
                  windowName: 'window1',
                  tabList: [
                    {
                      id: 1,
                      title: 'title1',
                      url: 'http://foo1',
                      favicon: 'dataurl1',
                      urlType: undefined,
                    },
                    {
                      id: undefined,
                      title: 'title2',
                      url: 'http://foo2',
                      favicon: 'dataurl2',
                      urlType: undefined,
                    },
                    {
                      id: 2,
                      title: 'Special Url',
                      url: 'http://special',
                      favicon: 'dataurl3',
                      urlType: testCase.urlTypeUi,
                    },
                  ],
                },
                {
                  // Default window name should be empty
                  windowName: '',
                  tabList: [
                    {
                      id: undefined,
                      title: 'title3',
                      url: 'http://foo3',
                      favicon: 'dataurl3',
                      urlType: undefined,
                    },
                  ],
                },
              ],
              result);
        });
  });

  test(
      'client delegate should properly translate mojom layer data for course' +
          'list',
      async () => {
        const result = await clientDelegateImpl.getInstance().getCourseList();
        assertDeepEquals(
            [
              {id: '1', name: 'course1', section: 'period1'},
              {id: '2', name: 'course2', section: ''},
            ],
            result);
      });

  test(
      'client delegate should properly translate mojom layer data for student' +
          'list',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().getStudentList('1');

        assertDeepEquals(
            [
              {id: '1', name: 'cat', email: 'email1', photoUrl: 'cdn1'},
              {id: '2', name: 'dog', email: 'email2', photoUrl: 'cdn2'},
            ],
            result);
      });

  test(
      'client delegate should properly translate mojom layer data for' +
          'assignment list',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().getAssignmentList('1');

        assertDeepEquals(
            [
              {
                title: 'assignment-title1',
                url: 'url1',
                lastUpdateTime: new Date(1000000),
                materials: [
                  {title: 'material-title-1', type: 0},
                  {title: 'material-title-2', type: 1},
                ],
                type: 0,
              },
              {
                title: 'assignment-title2',
                url: 'url2',
                lastUpdateTime: new Date(2000000),
                materials: [
                  {title: 'material-title-3', type: 2},
                  {title: 'material-title-4', type: 3},
                ],
                type: 1,
              },
            ],
            result);
      });

  urlTypeTestCases.forEach((testCase) => {
    test(
        `client delegate should translate data for creating session with ` +
            `urlType: ${testCase.name}`,
        async () => {
          remoteHandler.urlTypeMojo = testCase.urlTypeMojo;
          const result = await clientDelegateImpl.getInstance().createSession({
            sessionDurationInMinutes: 120,
            students: [
              {id: '1', name: 'cat', email: 'cat@gmail.com', photoUrl: 'cdn1'},
              {id: '2', name: 'dog', email: 'dog@gmail.com', photoUrl: 'cdn2'},
            ],
            studentsJoinViaCode: [],
            teacher: undefined,
            accessCode: undefined,
            sessionStartTime: undefined,
            onTaskConfig: {
              isLocked: true,
              isPaused: true,
              tabs: [
                {
                  tab: {
                    title: 'google',
                    url: 'http://google.com/',
                    favicon: 'data/image',
                  },
                  navigationType: 0,
                },
                {
                  tab: {
                    title: 'youtube',
                    url: 'http://youtube.com/',
                    favicon: 'data/image',
                  },
                  navigationType: 1,
                },
                {
                  tab: {
                    url: 'http://specialurl.com/',
                    title: 'special url',
                    favicon: 'data/image',
                    urlType: testCase.urlTypeUi,
                  },
                  navigationType: 0,
                },
              ],
            },
            captionConfig: {
              sessionCaptionEnabled: true,
              localCaptionEnabled: true,
              sessionTranslationEnabled: true,
            },
          });
          assertDeepEquals(1, result);
        });
  });

  urlTypeTestCases.forEach((testCase) => {
    test(
        `client delegate should properly translate get session with urlType: ${
            testCase.name}`,
        async () => {
          remoteHandler.urlTypeMojo = testCase.urlTypeMojo;
          const result = await clientDelegateImpl.getInstance().getSession();
          assertDeepEquals(
              {
                sessionConfig: {
                  sessionDurationInMinutes: 2,
                  sessionStartTime: new Date(1000000),
                  teacher: {
                    id: '0',
                    name: 'teacher',
                    email: 'teacher@gmail.com',
                    photoUrl: 'cdn0',
                  },
                  students: [
                    {
                      id: '1',
                      name: 'cat',
                      email: 'cat@gmail.com',
                      photoUrl: 'cdn1',
                    },
                    {
                      id: '2',
                      name: 'dog',
                      email: 'dog@gmail.com',
                      photoUrl: 'cdn2',
                    },
                  ],
                  studentsJoinViaCode: [
                    {
                      id: '3',
                      name: 'cat1',
                      email: 'cat1@gmail.com',
                      photoUrl: 'cdn3',
                    },
                  ],
                  accessCode: 'testCode',
                  onTaskConfig: {
                    isLocked: true,
                    isPaused: true,
                    tabs: [
                      {
                        tab: {
                          id: 1,
                          title: 'google',
                          url: 'http://google.com/',
                          favicon: 'data/image',
                          urlType: undefined,
                        },
                        navigationType: 0,
                      },
                      {
                        tab: {
                          id: undefined,
                          title: 'youtube',
                          url: 'http://youtube.com/',
                          favicon: 'data/image',
                          urlType: undefined,
                        },
                        navigationType: 1,
                      },
                      {
                        tab: {
                          id: undefined,
                          url: 'http://specialurl.com/',
                          title: 'special url',
                          favicon: 'data/image',
                          urlType: testCase.urlTypeUi,
                        },
                        navigationType: 0,
                      },
                    ],
                  },
                  captionConfig: {
                    sessionCaptionEnabled: true,
                    localCaptionEnabled: true,
                    sessionTranslationEnabled: true,
                  },
                },
                activity: [],
              },
              result);
        });
  });

  test(
      'client delegate should properly translate getSession with default value',
      () => {
        const session = {
          sessionDuration: {
            microseconds: 120000000n,
          },
          sessionStartTime: new Date(1000000),
          students: [],
          studentsJoinViaCode: [],
          onTaskConfig: {isLocked: false, isPaused: false, tabs: []},
          teacher: {
            id: '0',
            name: 'teacher',
            email: 'teacher@gmail.com',
            photoUrl: 'cdn0',
          },
          accessCode: null,
          captionConfig: {
            sessionCaptionEnabled: true,
            localCaptionEnabled: true,
            sessionTranslationEnabled: true,
          },
        };
        const result = getSessionConfigMojomToUI(session);
        assertDeepEquals(
            {
              sessionDurationInMinutes: 2,
              sessionStartTime: new Date(1000000),
              teacher: {
                id: '0',
                name: 'teacher',
                email: 'teacher@gmail.com',
                photoUrl: 'cdn0',
              },
              students: [],
              studentsJoinViaCode: [],
              onTaskConfig: {
                isLocked: false,
                isPaused: false,
                tabs: [],
              },
              accessCode: '',
              captionConfig: {
                sessionCaptionEnabled: true,
                localCaptionEnabled: true,
                sessionTranslationEnabled: true,
              },
            },

            result);
      });

  urlTypeTestCases.forEach((testCase) => {
    test(
        `client delegate should translate data for update on task config ` +
            `with urlType: ${testCase.name}`,
        async () => {
          remoteHandler.urlTypeMojo = testCase.urlTypeMojo;
          const result =
              await clientDelegateImpl.getInstance().updateOnTaskConfig({
                isLocked: true,
                isPaused: true,
                tabs: [
                  {
                    tab: {
                      title: 'google',
                      url: 'http://google.com/',
                      favicon: 'data/image',
                    },
                    navigationType: 0,
                  },
                  {
                    tab: {
                      title: 'youtube',
                      url: 'http://youtube.com/',
                      favicon: 'data/image',
                    },
                    navigationType: 1,
                  },
                  {
                    tab: {
                      title: 'special url',
                      url: 'http://specialurl.com/',
                      favicon: 'data/image',
                      urlType: testCase.urlTypeUi,
                    },
                    navigationType: 0,
                  },
                ],
              });
          assertTrue(result);
        });
  });

  test('client delegate should translate data for caption config', async () => {
    const result = await clientDelegateImpl.getInstance().updateCaptionConfig({
      sessionCaptionEnabled: true,
      localCaptionEnabled: true,
      sessionTranslationEnabled: true,
    });
    assertTrue(result);
  });

  test('client delegate should translate data for end session', async () => {
    const result = await clientDelegateImpl.getInstance().endSession();
    assertTrue(result);
  });

  test(
      'client delegate should translate data for extend session duration',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().extendSessionDuration(15);
        assertTrue(result);
      });

  test('client delegate should translate data for remove student', async () => {
    const result = await clientDelegateImpl.getInstance().removeStudent('1');
    assertTrue(result);
  });

  test('client delegate should translate data for add students', async () => {
    const result = await clientDelegateImpl.getInstance().addStudents([
      {id: '1', name: 'cat', email: 'cat@gmail.com', photoUrl: 'cdn1'},
      {id: '2', name: 'dog', email: 'dog@gmail.com', photoUrl: 'cdn2'},
    ]);
    assertTrue(result);
  });

  test('client delegate should translate data for student activity', () => {
    const activities = [
      {
        id: '1',
        activity: {
          studentStatusDetail: 3,
          isActive: true,
          activeTab: 'google',
          isCaptionEnabled: false,
          isHandRaised: false,
          joinMethod: 0,
          viewScreenSessionCode: 'abcd',
        },
      },
      {
        id: '2',
        activity: {
          studentStatusDetail: 2,
          isActive: false,
          activeTab: 'youtube',
          isCaptionEnabled: false,
          isHandRaised: false,
          joinMethod: 1,
          viewScreenSessionCode: null,
        },
      },
    ];
    const result = getStudentActivityMojomToUI(activities);
    assertDeepEquals(
        [
          {
            id: '1',
            studentActivity: {
              studentStatusDetail: 3,
              isActive: true,
              activeTab: 'google',
              isCaptionEnabled: false,
              isHandRaised: false,
              joinMethod: 0,
              viewScreenSessionCode: 'abcd',
            },
          },
          {
            id: '2',
            studentActivity: {
              studentStatusDetail: 2,
              isActive: false,
              activeTab: 'youtube',
              isCaptionEnabled: false,
              isHandRaised: false,
              joinMethod: 1,
              viewScreenSessionCode: undefined,
            },
          },
        ],
        result);
  });

  test('client delegate should translate data for set float', async () => {
    const result = await clientDelegateImpl.getInstance().setFloatMode(true);
    assertTrue(result);
  });

  test(
      'client delegate should translate data for submit access code',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().submitAccessCode('valid');
        assertDeepEquals(1, result);
      });

  test(
      'client delegate should return error for invalid access code',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().submitAccessCode('invalid');
        assertDeepEquals(2, result);
      });

  test('client delegate should translate data for network info', () => {
    const networks = [
      {state: 0, type: 0, name: 'network1', signalStrength: 50},
      {state: 1, type: 1, name: 'network2', signalStrength: 75},
    ];
    const result = getNetworkInfoMojomToUI(networks);
    assertDeepEquals(
        [
          {
            networkState: 0,
            networkType: 0,
            name: 'network1',
            signalStrength: 50,
          },
          {
            networkState: 1,
            networkType: 1,
            name: 'network2',
            signalStrength: 75,
          },
        ],
        result);
  });

  test(
      'client delegate should translate data for view student screen',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().viewStudentScreen('1');
        assertTrue(result);
      });

  test(
      'client delegate should translate data for ending a view screen session',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().endViewScreenSession('1');
        assertTrue(result);
      });

  test(
      'client delegate should translate data for updating a view screen' +
          ' session to active',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().setViewScreenSessionActive(
                '1');
        assertTrue(result);
      });



  test(
      'client delegate should respond correctly for retrieve user pref',
      async () => {
        const result = await clientDelegateImpl.getInstance().getUserPref(0);
        assertDeepEquals({stringValue: 'value'}, result);
      });

  test(
      'client delegate should respond correctly for set user pref',
      async () => {
        await clientDelegateImpl.getInstance().setUserPref(1, {value: {}});
      });
  test(
      'client delegate should respond correctly for set site permission',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().setSitePermission('1', 0, 0);
        assertTrue(result);
      });
  test(
      'client delegate should respond correctly for open feedback dialog',
      async () => {
        let openFeedbackDialogResponded = false;
        await clientDelegateImpl.getInstance().openFeedbackDialog().then(() => {
          openFeedbackDialogResponded = true;
        });
        assertTrue(openFeedbackDialogResponded);
      });

  test(
      'client delegate should respond correctly for refresh workbook',
      async () => {
        let refreshWorkbookResponded = false;
        await clientDelegateImpl.getInstance().refreshWorkbook().then(() => {
          refreshWorkbookResponded = true;
        });
        assertTrue(refreshWorkbookResponded);
      });

  test(
      'client delegate should translate data for renotify student',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().renotifyStudent('1');
        assertTrue(result);
      });

  test(
      'client delegate should translate data for start spotlight', async () => {
        let startSpotlightResponded = false;
        await clientDelegateImpl.getInstance().startSpotlight('1').then(() => {
          startSpotlightResponded = true;
        });
        assertTrue(startSpotlightResponded);
      });
});
