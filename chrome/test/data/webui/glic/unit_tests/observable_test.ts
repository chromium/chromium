
import {ObservableValue, Subject} from 'chrome://glic/observable.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ObservableTest', () => {
  setup(() => {});

  test('ObservableValue subscriber receives value when subscribed', () => {
    const observable = ObservableValue.withNoValue<number>();
    let valueReceived;
    const sub = observable.subscribe((value) => {
      valueReceived = value;
    });
    observable.assignAndSignal(1);
    sub.unsubscribe();
    observable.assignAndSignal(2);
    assertEquals(1, valueReceived);
  });

  test('ObservableValue gets completion callback when complete', () => {
    let reportedHasActiveSubscription;
    const observable =
        ObservableValue.withNoValue<number>((hasActiveSubscription) => {
          reportedHasActiveSubscription = hasActiveSubscription;
        });
    let completeReceived = false;
    const sub = observable.subscribe({
      next() {},
      complete() {
        completeReceived = true;
      },
    });
    observable.complete();
    assertTrue(completeReceived);
    sub.unsubscribe();
    assertEquals(false, reportedHasActiveSubscription);
  });

  test('ObservableValue does not get completion when unsubscribed', () => {
    const observable = ObservableValue.withNoValue<number>();
    let completeReceived = false;
    const sub = observable.subscribe({
      next() {},
      complete() {
        completeReceived = true;
      },
    });
    sub.unsubscribe();
    observable.complete();
    assertFalse(completeReceived);
  });

  test(
      'ObservableValue gets completion callback when' +
          ' complete before subscription',
      () => {
        const observable = ObservableValue.withNoValue<number>();
        observable.complete();
        let completeReceived = false;
        const sub = observable.subscribe({
          next() {},
          complete() {
            completeReceived = true;
          },
        });
        assertTrue(completeReceived);
        sub.unsubscribe();
      });


  test('ObservableValue assignAndSignal() while complete throws', () => {
    const observable = ObservableValue.withNoValue<number>();
    observable.complete();
    let thrownError;
    try {
      observable.assignAndSignal(1);
    } catch (e) {
      thrownError = e;
    }
    assertTrue(thrownError instanceof Error);
  });

  test('ObservableValue error() informs subscribers', () => {
    let reportedHasActiveSubscription;
    const observable =
        ObservableValue.withNoValue<number>((hasActiveSubscription) => {
          reportedHasActiveSubscription = hasActiveSubscription;
        });
    let reportedError;
    observable.subscribe({
      next() {},
      error(e) {
        reportedError = e;
      },
    });
    const error = new Error('test error');
    observable.error(error);
    assertEquals(error, reportedError);
    assertEquals(false, reportedHasActiveSubscription);
  });

  test('Subject subscriber gets value while subscribed', () => {
    const observable = new Subject<number>();
    let valueReceived;
    const sub = observable.subscribe((value) => {
      valueReceived = value;
    });
    observable.next(1);
    assertEquals(valueReceived, 1);
    sub.unsubscribe();
    observable.next(2);
    assertEquals(valueReceived, 1);
  });


  test('Subject gets completion callback when complete', () => {
    const observable = new Subject<number>();
    let completeReceived = false;
    const sub = observable.subscribe({
      next() {},
      complete() {
        completeReceived = true;
      },
    });
    observable.complete();
    assertTrue(completeReceived);
    sub.unsubscribe();
  });

  test('Subject does not get completion when unsubscribed', () => {
    const observable = new Subject<number>();
    let completeReceived = false;
    const sub = observable.subscribe({
      next() {},
      complete() {
        completeReceived = true;
      },
    });
    sub.unsubscribe();
    observable.complete();
    assertFalse(completeReceived);
  });

  test(
      'Subject gets completion callback when' +
          ' complete before subscription',
      () => {
        const observable = new Subject<number>();
        observable.complete();
        let completeReceived = false;
        const sub = observable.subscribe({
          next() {},
          complete() {
            completeReceived = true;
          },
        });
        assertTrue(completeReceived);
        sub.unsubscribe();
      });

  test('Subject next() while complete throws', () => {
    const observable = new Subject<number>();
    observable.complete();
    let thrownError;
    try {
      observable.next(1);
    } catch (e) {
      thrownError = e;
    }
    assertTrue(thrownError instanceof Error);
  });

  test('Subject error() informs subscribers', () => {
    const observable = new Subject<number>();
    let reportedError;
    observable.subscribe({
      next() {},
      error(e) {
        reportedError = e;
      },
    });
    const error = new Error('test error');
    observable.error(error);
    assertEquals(reportedError, error);
  });
});
