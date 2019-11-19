// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function showPrintJobTable() {
  chrome.printingMetrics.getPrintJobs(function(printJobs) {
    const tbody = document.createElement('tbody');

    for (let i = 0; i < printJobs.length; ++i) {
      const columnValues = [
        printJobs[i].title, printJobs[i].status,
        new Date(printJobs[i].completionTime), printJobs[i].numberOfPages,
        printJobs[i].printer.name, printJobs[i].printer.uri,
        printJobs[i].printer.source, printJobs[i].settings.color,
        printJobs[i].settings.duplex, printJobs[i].settings.mediaSize.width,
        printJobs[i].settings.mediaSize.height, printJobs[i].settings.copies
      ];

      let tr = document.createElement('tr');
      for (columnValue of columnValues) {
        const td = document.createElement('td');
        td.appendChild(document.createTextNode(columnValue));
        td.setAttribute('align', 'center');
        tr.appendChild(td);
      }
      tbody.appendChild(tr);
    }

    const table = document.getElementById('printJobsTable');
    table.appendChild(tbody);
  });
}

document.addEventListener('DOMContentLoaded', function() {
  showPrintJobTable();
});
