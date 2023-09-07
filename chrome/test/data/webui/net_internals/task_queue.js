// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

/**
 * This class allows multiple Tasks to be queued up to be run sequentially.
 */
export class TaskQueue {
  constructor() {
    // Test fixture for passing to tasks.
    this.tasks_ = [];
    this.whenDonePromise_ = null;
  }

  /**
   * Adds a Task to the end of the queue.  Each Task may only be added once
   * to a single queue.  Also passes the text fixture to the Task.
   * @param {Task}: task The Task to add.
   */
  addTask(task) {
    this.tasks_.push(task);
    task.setTaskQueue_(this);
  }

  /**
   * Adds a Task to the end of the queue.  The task will call the provided
   * function, and then complete.
   * @param {function}: taskFunction The function the task will call.
   */
  addFunctionTask(taskFunction) {
    this.addTask(new CallFunctionTask(taskFunction));
  }

  /**
   * Starts running the Tasks in the queue, passing its arguments, if any,
   * to the first Task's start function.  May only be called once.
   * @return {!Promise} Promise that resolves when all tasks have run.
   */
  run() {
    assertEquals(null, this.whenDonePromise_);
    this.whenDonePromise_ = new PromiseResolver();
    this.runNextTask(Array.prototype.slice.call(arguments));
    return this.whenDonePromise_.promise;
  }

  /**
   * If there are any Tasks in |tasks_|, removes the first one and runs it.
   * Otherwise, resolves the promise returned by run().
   * @param {array} argArray arguments to be passed on to next Task's start
   *     method.  May be a 0-length array.
   */
  runNextTask(argArray) {
    assertTrue(!!this.whenDonePromise_);

    if (this.tasks_.length > 0) {
      const nextTask = this.tasks_.shift();
      nextTask.start.apply(nextTask, argArray);
    } else {
      this.whenDonePromise_.resolve();
      this.whenDonePromise_ = null;
    }
  }
}

  /**
   * A Task that can be added to a TaskQueue.  A Task is started with a call to
   * the start function, and must call its own onTaskDone when complete.
   */
export class Task {
  constructor() {
    this.taskQueue_ = null;
    this.isDone_ = false;
  }

  /**
   * Starts running the Task.  Only called once per Task, must be overridden.
   * Any arguments passed to the last Task's onTaskDone, or to run (If the
   * first task) will be passed along.
   */
  start() {
    assertNotReached('Start function not overridden.');
  }

  /**
   * @return {bool} True if this task has completed by calling onTaskDone.
   */
  isDone() {
    return this.isDone_;
  }

  /**
   * Sets the TaskQueue used by the task in the onTaskDone function.  May only
   * be called by the TaskQueue.
   * @param {TaskQueue}: taskQueue The TaskQueue |this| has been added to.
   */
  setTaskQueue_(taskQueue) {
    assertEquals(null, this.taskQueue_);
    this.taskQueue_ = taskQueue;
  }

  /**
   * Must be called when a task is complete, and can only be called once for a
   * task.  Runs the next task, if any, passing along all arguments.
   */
  onTaskDone() {
    assertFalse(this.isDone_);
    this.isDone_ = true;

    // Run the next task in the queue.
    this.taskQueue_.runNextTask(Array.prototype.slice.call(arguments));
  }
}

/**
 * A Task that can be added to a TaskQueue.  A Task is started with a call to
 * the start function, and must call its own onTaskDone when complete.
 * @extends {Task}
 * @constructor
 */
class CallFunctionTask extends Task {
  constructor(taskFunction) {
    super();
    assertEquals('function', typeof taskFunction);
    this.taskFunction_ = taskFunction;
  }

  /**
   * Runs the function and then completes.  Passes all arguments, if any,
   * along to the function.
   */
  start() {
    this.taskFunction_.apply(null, Array.prototype.slice.call(arguments));
    this.onTaskDone();
  }
}
