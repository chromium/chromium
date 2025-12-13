// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Hot Tip! Generate a tsconfig.json file to get language server support. Run:
// ash/webui/personalization_app/tools/gen_tsconfig.py --root_out_dir out/pc \
//   --gn_target chrome/test/data/webui/glic:build_ts

import {WebClientMode} from '/glic/glic_api/glic_api.js';
import type {GlicBrowserHost, GlicHostRegistry, GlicWebClient, Observable, OpenPanelInfo, PanelOpeningData, PanelStateKind} from '/glic/glic_api/glic_api.js';
import {ObservableValue, Subject, type Subscriber} from '/glic/observable.js';

import {createGlicHostRegistryOnLoad} from '../api_boot.js';

let maxTimeoutEndTime = performance.now() + 10000;

export function getTestName(): string|null {
  let testName = new URL(window.location.href).searchParams.get('test');
  if (testName?.startsWith('DISABLED_')) {
    testName = testName.substring('DISABLED_'.length);
  }
  const lastSlashIndex = testName?.lastIndexOf('/');
  if (lastSlashIndex !== -1) {
    testName = testName ? testName.substring(0, lastSlashIndex) : null;
  }
  return testName;
}

export function mapObservable<S, T>(src: Observable<S>, mapping: (s: S) => T) {
  const result = new Subject<T>();
  src.subscribe(
      (v) => {
        result.next(mapping(v));
      },
  );
  return result;
}

// Creates a queue of promises from an observable.
export class SequencedSubscriber<T> {
  private signals: Array<PromiseWithResolvers<T>> = [];
  private readIndex = 0;
  private writeIndex = 0;
  private subscriber: Subscriber;

  // The last value read from `next()`, or undefined if none was read.
  current: {some: T}|undefined;

  // A promise that resolves when the observable is completed.
  readonly completed: Promise<void>;

  constructor(observable: Observable<T>) {
    const completedResolvers = Promise.withResolvers<void>();
    this.completed = completedResolvers.promise;
    this.subscriber = observable.subscribeObserver!({
      next: this.change.bind(this),
      complete: completedResolvers.resolve,
    });
  }
  async next(): Promise<T> {
    // Wrapping the returned value with `waitFor` improves failure logs
    // on timeout.
    this.current = {
      some: await waitFor(this.getSignal(this.readIndex++).promise),
    };
    return this.current.some;
  }

  /** Returns true if all values have been read. */
  isEmpty(): boolean {
    return this.readIndex >= this.writeIndex;
  }
  unsubscribe() {
    this.subscriber.unsubscribe();
  }
  waitForValue(targetValue: T) {
    return this.waitFor(v => v === targetValue);
  }
  async waitFor(condition: (v: T) => boolean): Promise<T> {
    let lastValueSaw: {some: T}|undefined = undefined;
    if (this.current !== undefined) {
      if (condition(this.current.some)) {
        return this.current.some;
      }
      lastValueSaw = {some: this.current.some};
    }

    while (true) {
      let val;
      try {
        val = await this.next();
      } catch (e) {
        if (lastValueSaw !== undefined) {
          console.warn(`waitFor() failed, last value saw was ${
              JSON.stringify(lastValueSaw)}`);
        } else {
          console.warn(`waitFor() failed, saw no values emitted`);
        }
        throw e;
      }
      if (condition(val)) {
        return val;
      }
      lastValueSaw = {some: val};
      console.info(`waitFor saw and ignored ${JSON.stringify(val)}`);
    }
  }
  private change(val: T) {
    this.getSignal(this.writeIndex++).resolve(val);
  }
  private getSignal(index: number) {
    while (this.signals.length <= index) {
      this.signals.push(Promise.withResolvers<T>());
    }
    return this.signals[index]!;
  }
}

export function observeSequence<T>(observable: Observable<T>):
    SequencedSubscriber<T> {
  return new SequencedSubscriber(observable);
}

// A dummy web client.
export class WebClient implements GlicWebClient {
  host?: GlicBrowserHost;
  firstOpened = Promise.withResolvers<void>();
  initializedPromise = Promise.withResolvers<void>();
  onNotifyPanelWasClosed: () => void = () => {};
  panelOpenState = ObservableValue.withValue<boolean>(false);
  panelOpenStateKind = ObservableValue.withNoValue<PanelStateKind>();
  panelOpenData = ObservableValue.withNoValue<PanelOpeningData>();

  async initialize(glicBrowserHost: GlicBrowserHost): Promise<void> {
    this.host = glicBrowserHost;
    this.initializedPromise.resolve();
  }

  async notifyPanelWillOpen(panelOpeningData: PanelOpeningData):
      Promise<OpenPanelInfo> {
    this.panelOpenState.assignAndSignal(true);
    this.panelOpenStateKind.assignAndSignal(
        checkDefined(panelOpeningData.panelState?.kind));
    this.panelOpenData.assignAndSignal(panelOpeningData);
    this.firstOpened.resolve();

    const openPanelInfo: OpenPanelInfo = {
      startingMode: WebClientMode.TEXT,
    };
    return openPanelInfo;
  }

  async notifyPanelWasClosed(): Promise<void> {
    this.onNotifyPanelWasClosed();
    this.panelOpenState.assignAndSignal(false);
  }

  waitForFirstOpen(): Promise<void> {
    return this.firstOpened.promise;
  }

  waitForInitialize(): Promise<void> {
    return this.initializedPromise.promise;
  }
}

export interface TestStepper {
  nextStep(data: any): Promise<void>;
}

export const glicHostRegistry = Promise.withResolvers<GlicHostRegistry>();

export class ApiTestFixtureBase {
  private clientValue?: WebClient;
  private testStepLabel: string;
  private testStepCount: number;
  // Test parameters passed to `ExecuteJsTest()`. This is undefined until
  // ExecuteJsTest() is called.
  testParams: any;
  constructor(protected testStepper: TestStepper) {
    this.testStepCount = 1;
    this.testStepLabel = `step #${this.testStepCount} (single or first)`;
  }

  // Return to the C++ side, and wait for it to call ContinueJsTest() to
  // continue execution in the JS test. Optionally, pass data to the C++ side.
  async advanceToNextStep(data?: any): Promise<void> {
    this.testStepLabel =
        `in between steps ${this.testStepCount} and ${this.testStepCount + 1}`;
    await this.testStepper.nextStep(data);
    this.testStepCount += 1;
    this.testStepLabel = `step #${this.testStepCount}`;
  }

  // Sets up the web client. This is called when the web contents loads,
  // before `ExecuteJsTest()`.
  async setUpClient() {
    this.setUpWithClient(this.createWebClient());
  }

  async setUpWithClient(client: WebClient) {
    const registry = await glicHostRegistry.promise;
    this.clientValue = client;
    await registry.registerWebClient(client);
    assertTrue(!!this.clientValue);
  }

  // Performs setup for the test, called after `setUpClient()`.
  async setUpTest() {}

  // Creates the web client. Allows a fixture to use a different implementation.
  createWebClient() {
    return new WebClient();
  }

  get host(): GlicBrowserHost {
    const h = this.client.host;
    assertTrue(!!h);
    return h;
  }

  get client(): WebClient {
    assertTrue(!!this.clientValue);
    return this.clientValue;
  }

  getStepLabel(): string {
    return this.testStepLabel;
  }

  getStepCount(): number {
    return this.testStepCount;
  }

  async testAllTestsAreRegistered() {
    const allNames = [];
    for (const fixture of testRunner.testFixtures) {
      allNames.push(...Object.getOwnPropertyNames(fixture.prototype)
                        .filter(name => name.startsWith('test')));
    }
    await this.advanceToNextStep(allNames);
  }
}

function findTestFixture(testFixtures: any[], testName: string): any {
  for (const fixture of testFixtures) {
    if (Object.getOwnPropertyNames(fixture.prototype).includes(testName)) {
      return fixture;
    }
  }
  // testAllTestsAreRegistered is provided by the fixture base class.
  if (testName === 'testAllTestsAreRegistered') {
    return testFixtures[0];
  }
  return undefined;
}

// Result of running a test.
type TestResult =
    // The test completed successfully.
    'pass'|
    // A test step is complete. `continueApiTest()` needs to be called to
    // finish. The second value is the data passed to `nextStep()`.
    {id: 'next-step', payload: any}|
    // Any other string is an error.
    string;

// Runs a test.
class TestRunner implements TestStepper {
  nextStepPromise = Promise.withResolvers<{id: 'next-step', payload: any}>();
  continuePromise = Promise.withResolvers<void>();
  fixture: ApiTestFixtureBase|undefined;
  testDone: Promise<void>|undefined;
  testFound = false;
  stepFailures: ApiTestError[] = [];
  constructor(private testName: string, public testFixtures: any[]) {
    console.info(`TestRunner(${testName})`);
  }

  async setUp() {
    let fixtureCtor = findTestFixture(this.testFixtures, this.testName);
    if (!fixtureCtor) {
      // Note: throwing an exception here will not make it to the c++ side.
      // Wait until later to throw an error.
      console.error(`Test case not found: ${this.testName}`);
      this.testName = 'testDoNothing';
      fixtureCtor = findTestFixture(this.testFixtures, this.testName);
    } else {
      this.testFound = true;
    }
    this.fixture = (new fixtureCtor(this)) as ApiTestFixtureBase;
    return await this.fixture.setUpClient();
  }

  recordTestFailure(error: ApiTestError) {
    this.stepFailures.push(error);
  }

  // Sets up the test and starts running it.
  async run(maxTimeoutMs: number, payload: any): Promise<TestResult> {
    assertTrue(this.testFound, `Test not found: "${this.testName}"`);
    maxTimeoutEndTime = performance.now() + maxTimeoutMs;
    console.info(`Running test ${this.testName} with payload ${
        JSON.stringify(payload)}`);
    this.fixture!.testParams = payload;
    await this.fixture!.setUpTest();
    this.testDone = (this.fixture as any)[this.testName]() as Promise<void>;
    return this.continueTest();
  }

  // If `run()` or `stepComplete()` returns 'next-step', this function is called
  // to continue running the test.
  stepComplete(payload: any): Promise<TestResult> {
    console.info(`Continue ${this.testName}`);
    if (payload !== null) {
      this.fixture!.testParams = payload;
    }
    this.nextStepPromise = Promise.withResolvers();
    const continueResolve = this.continuePromise.resolve;
    this.continuePromise = Promise.withResolvers();
    continueResolve();
    return this.continueTest();
  }

  private async continueTest(): Promise<TestResult> {
    try {
      const result =
          await Promise.race([this.testDone, this.nextStepPromise.promise]);
      if (this.stepFailures.length > 0) {
        // One or more failures occurred during the test but they were not
        // raised as an error. Report the first non-raised failure.
        const e: ApiTestError = this.stepFailures[0] as ApiTestError;
        console.error(
            `Test ${this.testName} failed at ${
                this.fixture!.getStepLabel()}.\n` +
            await improveStackTrace(e.stack ?? ''));
        return `Failed at ${this.fixture!.getStepLabel()} ` +
            `due to (captured error): ${e}`;
      }
      if (result && typeof result === 'object' &&
          result['id'] === 'next-step') {
        return result;
      }
    } catch (e) {
      if (e instanceof Error) {
        console.error(
            `Test ${this.testName} failed at ${
                this.fixture!.getStepLabel()}.\n` +
            await improveStackTrace(e.stack ?? ''));
      }
      return `Failed at ${this.fixture!.getStepLabel()} due to: ${e}`;
    } finally {
      this.stepFailures = [];
    }
    return 'pass';
  }

  // TestStepper implementation.
  nextStep(payload: any): Promise<void> {
    console.info(`Waiting to continue to step #${
        this.fixture!.getStepCount() + 1} in test ${this.testName}...`);
    payload = payload ?? {};  // undefined is not serializable to base::Value.
    this.nextStepPromise.resolve({id: 'next-step', payload});
    return this.continuePromise.promise;
  }
}

// Adds js source lines to the stack trace.
async function improveStackTrace(stack: string) {
  const outLines: string[] = [];
  const contextLines = 2;  // Must be >= 1
  let stackLevel = 0;
  for (const line of stack.split('\n')) {
    const m = line.match(/^\s+at.*\((.*):(\d+):(\d+)\)$/);
    if (m) {
      try {
        const [file, lineNo, column] = m.slice(1);
        const response = await fetch(file!);
        const text = await response.text();
        const lines = text.split('\n');
        const failureLineNo = Number(lineNo) - 1;
        outLines.push(`[${stackLevel}] ${line.trim()}:`);
        const spacePrefixedIntroLines =
            lines.slice(failureLineNo - contextLines, failureLineNo)
                .map((l) => ' |' + l);
        outLines.push(...spacePrefixedIntroLines);
        const lineStr = lines[failureLineNo];
        outLines.push(` ├>${lineStr}`);
        outLines.push(` ├╌${'╌'.repeat(Number(column) - 1)}^`);
        const spacePrefixedOutroLines =
            lines.slice(failureLineNo + 1, failureLineNo + contextLines + 1)
                .map((l) => ' |' + l);
        outLines.push(...spacePrefixedOutroLines);
        outLines.push('');
      } catch (e) {
        outLines.push(`${line}`);
      }
      stackLevel += 1;
    } else {
      outLines.push(line);
    }
  }
  outLines.push('');
  return outLines.join('\n');
}

let testRunner: TestRunner;

export async function testMain(testFixtures: any[]) {
  if (getTestName() !== 'testNoBootstrap') {
    console.info('api_test waiting for GlicHostRegistry');
    glicHostRegistry.resolve(await createGlicHostRegistryOnLoad());
  }

  // If no test is selected, load a client that does nothing.
  // This is present because test.html is used as a dummy test client in
  // some tests.
  testRunner = new TestRunner(getTestName() ?? 'testDoNothing', testFixtures);
  await testRunner.setUp();

  (window as any).runApiTest =
      (maxTimeoutMs: number, payload: any): Promise<TestResult> => {
        return testRunner.run(maxTimeoutMs, payload);
      };

  (window as any).continueApiTest = (payload: any): Promise<TestResult> => {
    return testRunner.stepComplete(payload);
  };
}

/** Error type for causing API test failures. */
export class ApiTestError extends Error {
  constructor(message: string) {
    super(message);
    testRunner.recordTestFailure(this);
  }
}

export type ComparableValue = boolean|string|number|undefined|null;

export function assertTrue(x: boolean, message?: string): asserts x {
  if (!x) {
    throw new ApiTestError(
        `assertTrue failed: '${x}' is not true. ${message ?? ''}`);
  }
}

export function assertFalse(x: boolean, message?: string): asserts x is false {
  if (x) {
    throw new ApiTestError(
        `assertFalse failed: '${x}' is not false. ${message ?? ''}`);
  }
}

export function assertDefined<T>(x: T|undefined, message?: string): asserts x {
  if (x === undefined) {
    throw new Error(`assertDefined failed. ${message ?? ''}`);
  }
}

export function assertUndefined<T>(x: T|undefined, message?: string) {
  if (x !== undefined) {
    throw new Error(`assertUndefined failed. ${message ?? ''}`);
  }
}

export function assertEquals(
    a: ComparableValue, b: ComparableValue, message?: string) {
  if (a !== b) {
    throw new ApiTestError(
        `assertEquals failed: '${a}' !== '${b}'. ${message ?? ''}`);
  }
}

export function assertNotEquals(
    a: ComparableValue, b: ComparableValue, message?: string) {
  if (a === b) {
    throw new ApiTestError(
        `assertNotEquals failed: '${a}' === '${b}'. ${message ?? ''}`);
  }
}

export function checkDefined<T>(v: T|undefined, message?: string): T {
  if (v === undefined) {
    throw new ApiTestError(
        `checkDefined: value is undefined. ${message ?? ''}`);
  }
  return v;
}

export function sleep(timeoutMs: number): Promise<void> {
  return new Promise((resolve) => {
    window.setTimeout(resolve, timeoutMs);
  });
}

export function getTimeout(timeoutMs?: number): number {
  if (timeoutMs === undefined) {
    return Math.max(0, maxTimeoutEndTime - performance.now());
  }
  return timeoutMs;
}

// Waits for a promise to resolve. If the timeout is reached first, throws an
// exception. Note this is useful because if the test times out in the normal
// way, we do not receive a very useful error.
export async function waitFor<T>(
    value: Promise<T>, timeoutMs?: number, message?: string): Promise<T> {
  const timeoutResult = Symbol();
  const result = await Promise.race(
      [value, sleep(getTimeout(timeoutMs)).then(() => timeoutResult)]);
  if (result === timeoutResult) {
    throw new ApiTestError(`waitFor timed out. ${message ?? ''}`);
  }
  return value;
}


// Run until `condition()` returns a truthy value. Throws an exception if the
// timeout is reached first. Otherwise, this returns the value returned by
// condition.
export async function runUntil<T>(
    condition: () => T | PromiseLike<T>, timeoutMs?: number,
    message?: string): Promise<NonNullable<T>> {
  timeoutMs = getTimeout(timeoutMs);
  const sleepMs = getTimeout(timeoutMs) / 20;
  const timeout = performance.now() + timeoutMs;
  while (performance.now() < timeout) {
    const result = await condition();
    if (result) {
      return result;
    }
    await sleep(sleepMs);
  }
  throw new ApiTestError(`runUntil timed out. ${message ?? ''}`);
}

export function readStream(stream: ReadableStream<Uint8Array>):
    Promise<Uint8Array> {
  return new Response(stream).bytes();
}

export async function assertRejects<T>(
    promise: Promise<T>,
    options?: {withErrorMessage?: string}): Promise<string|undefined> {
  return promise.then(
      () => {
        // The promise should have been rejected.
        throw new ApiTestError('Promise not rejected.');
      },
      (e) => {
        assertTrue(
            e instanceof Error,
            'JS test harness does not support non-Error rejection objects');
        const errorMessage = (e as Error).message;
        if (options?.withErrorMessage !== undefined) {
          assertEquals(options.withErrorMessage, errorMessage);
        }
        return errorMessage;
      });
}
