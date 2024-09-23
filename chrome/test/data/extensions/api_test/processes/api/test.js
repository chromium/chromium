// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Processes API test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.Processes

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var assertFalse = chrome.test.assertFalse;
var listenOnce = chrome.test.listenOnce;

var tabs = [];
var hangingTabProcess = -1;

function createTab(index, url) {
  chrome.tabs.create({"url": url}, pass(function(tab) {
    tabs[index] = tab;
  }));
}

var getProcessId = chrome.processes.getProcessIdForTab;

function pageUrl(letter) {
  return chrome.runtime.getURL(letter + ".html");
}

function dumpProcess(process) {
  console.log("id          " + process.id);
  console.log("osProcId    " + process.osProcessId);
  console.log("type        " + process.type);
  console.log("profile     " + process.profile);
  console.log("tasks       " + process.tasks);
  for (var i = 0; i < process.tasks.length; ++i) {
    console.log("task["+ i + "].title       " + process.tasks[i].title);
    if ("tabId" in process.tasks[i])
      console.log("task["+ i + "].tabId       " + process.tasks[i].tabId);
  }
  console.log("cpu         " + process.cpu);
  console.log("privMem     " + process.privateMemory);
  console.log("network     " + process.network);
  console.log("jsMemAlloc  " + process.jsMemoryAllocated);
  console.log("jsMemUsed   " + process.jsMemoryUsed);
  console.log("sqliteMem   " + process.sqliteMemory);
  console.log("naclDebugPort " + process.naclDebugPort);
  if ("imageCache" in process) {
    console.log("imageCache.size      " + process.imageCache.size);
    console.log("imageCache.liveSize  " + process.imageCache.liveSize);
  }
  if ("scriptCache" in process) {
    console.log("scriptCache.size     " + process.scriptCache.size);
    console.log("scriptCache.liveSize " + process.scriptCache.liveSize);
  }
  if ("cssCache" in process) {
    console.log("cssCache.size        " + process.cssCache.size);
    console.log("cssCache .liveSize   " + process.cssCache.liveSize);
  }
}

function validateProcessProperties(process, updating, memory_included) {
  // Always present.
  assertTrue("id" in process);
  assertTrue("naclDebugPort" in process);
  assertTrue("osProcessId" in process);
  assertTrue("type" in process);
  assertTrue("profile" in process);
  assertTrue("tasks" in process);
  assertTrue("title" in process.tasks[0]);

  // Present if onUpdate(WithMemory) listener is registered.
  assertEq(("cpu" in process), updating);
  assertEq(("network" in process), updating);

  // Present if memory details are requested.
  assertEq(("privateMemory" in process), memory_included);

  // sqliteMemory is only reported for the browser process
  if (process.type == "browser") {
    assertEq(("sqliteMemory" in process), updating);
  } else if (process.type == "renderer") {
    // The rest are not present in the browser process
    assertEq(("jsMemoryAllocated" in process), updating);
    assertEq(("jsMemoryUsed" in process), updating);
    assertEq(("imageCache" in process), updating);
    assertEq(("scriptCache" in process), updating);
    assertEq(("cssCache" in process), updating);
  }
}

chrome.test.runTests([
  function setupProcessTests() {
    // Open 4 tabs for test, then wait and create a 5th
    createTab(0, "about:blank");
    createTab(1, pageUrl("a"));
    createTab(2, pageUrl("b"));
    createTab(3, "chrome://newtab/");

    // Wait for all loads to complete.
    var completedCount = 0;
    var onUpdatedCompleted = chrome.test.listenForever(
      chrome.tabs.onUpdated,
      function(changedTabId, changeInfo, changedTab) {
        if (changedTab.status == "complete") {
          completedCount++;

          // Once the NTP finishes loading, create another one.  This ensures
          // both NTPs end up in the same process.
          if (changedTabId == tabs[3].id)
            createTab(4, "chrome://newtab/");
        }

        // Once all tabs are done loading, continue with the next test.
        if (completedCount == 5)
          onUpdatedCompleted();
      }
    );

  },

  function extensionPageInOwnProcess() {
    getProcessId(tabs[0].id, pass(function(pid0) {
      getProcessId(tabs[1].id, pass(function(pid1) {
        // about:blank and extension page should not share a process
        assertTrue(pid0 != pid1);
      }));
    }));
  },

  function extensionPagesShareProcess() {
    getProcessId(tabs[1].id, pass(function(pid1) {
      getProcessId(tabs[2].id, pass(function(pid2) {
        // Pages from same extension should share a process
        assertEq(pid1, pid2);
      }));
    }));
  },

  function extensionPagesMatchTabs() {
    getProcessId(tabs[1].id, pass(function(pid1) {
      getProcessId(tabs[2].id, pass(function(pid2) {
        // Pages from same extension should share a process
        assertEq(pid1, pid2);
        chrome.processes.getProcessInfo(pid1, false,
            function(pl1) {
              chrome.processes.getProcessInfo(pid2, false,
                  function (pl2) {
                    var proc1 = pl1[pid1];
                    var proc2 = pl2[pid2];
                    assertTrue(proc1.tasks.length == proc2.tasks.length);
                    for (var i = 0; i < proc1.tasks.length; ++i) {
                      assertEq(proc1.tasks[i], proc2.tasks[i]);
                    }
                  });
            });
      }));
    }));
  },

  function newTabPageInOwnProcess() {
    getProcessId(tabs[0].id, pass(function(pid0) {
      getProcessId(tabs[3].id, pass(function(pid3) {
        // NTP should not share a process with current tabs
        assertTrue(pid0 != pid3);
      }));
    }));
  },

  function newTabPagesShareProcess() {
    getProcessId(tabs[3].id, pass(function(pid3) {
      getProcessId(tabs[4].id, pass(function(pid4) {
        // Multiple NTPs should share a process
        assertEq(pid3, pid4);
      }));
    }));
  },

  function idsInUpdateEvent() {
    listenOnce(chrome.processes.onUpdated, function(processes) {
      // onUpdated should return a valid dictionary of processes,
      // indexed by process ID.
      var pids = Object.keys(processes);
      // There should be at least 5 processes: 1 browser, 1 extension, and 3
      // renderers (for the 5 tabs).
      assertTrue(pids.length >= 5, "Unexpected size of pids");

      // Should be able to look up process object by ID.
      assertTrue(processes[pids[0]].id == pids[0]);
      assertTrue(processes[pids[0]].id != processes[pids[1]].id);

      getProcessId(tabs[0].id, pass(function(pidTab0) {
        // Process ID for tab 0 should be listed in pids.
        assertTrue(processes[pidTab0] != undefined, "Undefined Process");
        assertEq("renderer", processes[pidTab0].type, "Tab0 is not renderer");
      }));
    });
  },

  function typesInUpdateEvent() {
    listenOnce(chrome.processes.onUpdated, function(processes) {
      // Check types: 1 browser, 3 renderers, and 1 extension
      var browserCount = 0;
      var rendererCount = 0;
      var extensionCount = 0;
      var otherCount = 0;
      for (pid in processes) {
        switch (processes[pid].type) {
          case "browser":
            browserCount++;
            break;
          case "renderer":
            rendererCount++;
            break;
          case "extension":
            extensionCount++;
            break;
          default:
            otherCount++;
        }
      }
      assertEq(1, browserCount);
      assertTrue(rendererCount >= 3);
      assertTrue(extensionCount >= 1);
    });
  },

  function propertiesOfProcesses() {
    listenOnce(chrome.processes.onUpdated, function(processes) {
      for (pid in processes) {
        var process = processes[pid];
        validateProcessProperties(process, true, false);
      }
    });
  },

  function propertiesOfProcessesWithMemory() {
    listenOnce(chrome.processes.onUpdatedWithMemory,
        function(processes) {
          for (pid in processes) {
            var process = processes[pid];
            validateProcessProperties(process, true, true);
          }
        });
  },

  function terminateProcess() {
    listenOnce(chrome.processes.onExited,
      function(processId, type, code) {
        assertTrue(processId > 0);
      });
    getProcessId(tabs[4].id, function(pid0) {
      chrome.processes.terminate(pid0, function(killed) {
        chrome.test.assertTrue(killed);
      });
    });
  },

  function terminateProcessNonExisting() {
    chrome.processes.terminate(31337, fail("Process not found: 31337."));
  },

  function testOnCreated() {
    listenOnce(chrome.processes.onCreated, function(process) {
      assertTrue("id" in process, "process doesn't have id property");
      // We don't report the creation of the browser process, hence process.id
      // is expected to be > 0.
      assertTrue(process.id > 0, "id is not positive " + process.id);
    });
    createTab(5, "chrome://newtab/");
  },

  // DISABLED: crbug.com/345411
  // Hangs consistently (On Windows).
  /*
  function testOnExited() {
    listenOnce(chrome.processes.onExited,
        function(processId, type, code) {
      assertTrue(type >= 0 && type < 5);
    });
    chrome.tabs.create({"url": "http://google.com/"}, pass(function(tab) {
      chrome.tabs.remove(tab.id);
    }));
  },
  */

  function testGetProcessInfoList() {
     getProcessId(tabs[0].id, pass(function(pidTab0) {
       getProcessId(tabs[1].id, pass(function(pidTab1) {
         chrome.processes.getProcessInfo([pidTab0, pidTab1], false,
                                         pass(function(processes) {
           assertTrue(Object.keys(processes).length == 2);
         }));
       }));
     }));
  },

  function testGetProcessInfoSingle() {
    chrome.processes.getProcessInfo(0, false, pass(function(processes) {
      assertTrue(Object.keys(processes).length == 1);
    }));
  },

  function testGetProcessInfo() {
    chrome.processes.getProcessInfo([], false, pass(function(processes) {
      assertTrue(Object.keys(processes).length >= 1);
      for (pid in processes) {
        var process = processes[pid];
        validateProcessProperties(process, false, false);
        assertFalse("privateMemory" in process);
      }
    }));
  },

  function testGetProcessInfoWithMemory() {
     chrome.processes.getProcessInfo(0, true, pass(function(processes) {
       for (pid in processes) {
         var process = processes[pid];
         validateProcessProperties(process, false, true);
         assertTrue("privateMemory" in process);
       }
     }));
  },

  function testOnUnresponsive() {
    listenOnce(chrome.processes.onUnresponsive, function(process) {
      assertTrue(process.id == hangingTabProcess);
      // actually kill the process, just to make sure it won't hang the test
      chrome.processes.terminate(process.id, function(killed) {
          chrome.test.assertTrue(killed);
      });
    });
    chrome.tabs.create({"url": "chrome://hang" }, function(tab) {
      getProcessId(tab.id, function(pid0) {
        hangingTabProcess = pid0;
      });
      chrome.tabs.update(tab.id, { "url": "chrome://flags" });
    });
  }
]);
