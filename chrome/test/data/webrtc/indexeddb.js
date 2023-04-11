/**
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** @private */
var kDatabaseName = 'WebRTC-Database';

/**
 * The one and only |IDBDatabase| in this page.
 * @private
 */
var gDatabase = null;

/**
 * Set by |generateAndCloneCertificate|.
 */
var gCertificate = null;
var gCertificateClone = null;

// Public interface to tests.

function openDatabase() {
  if (gDatabase !== null)
    throw new Error('The database is already open.');
  var reqOpen = indexedDB.open(kDatabaseName);
  reqOpen.onupgradeneeded = function() {
    // This happens before |onsuccess| if the database is new or its version is
    // updated. Create object stores.
    var db = reqOpen.result;
    var certStore = db.createObjectStore('certificates', { keyPath: 'id' });
  };
  return new Promise((resolve, reject) => {
    reqOpen.onsuccess = function() {
      if (gDatabase !== null)
        return reject(new Error('The database is already open.'));
      gDatabase = reqOpen.result;
      return resolve(logAndReturn('ok-database-opened'));
    }
    reqOpen.onerror = function() {
      reject(new Error('The database could not be opened. Error: ' +
        reqOpen.error));
    }
  });
}

function closeDatabase() {
  if (gDatabase === null)
    throw new Error('The database is already closed.');
  gDatabase.close();
  gDatabase = null;
  return logAndReturn('ok-database-closed');
}

function deleteDatabase() {
  if (gDatabase !== null)
    throw new Error('The database should be closed before deleting.');
  var reqDelete = indexedDB.deleteDatabase(kDatabaseName);
  return new Promise((resolve, reject) => {
    reqDelete.onsuccess = function () {
      resolve(logAndReturn('ok-database-deleted'));
    };
    reqDelete.onerror = function () {
      reject(new Error('The database could not be deleted. Error: '
        + reqDelete.error));
    };
  });
}

/**
 * Generates a certificate and clones it by saving and loading it to the
 * database (requires database to be open, see |openDatabase|). After returning
 * successfully to the test, the global variables |gCertificate| and
 * |gCertificateClone| have been set.
 * @param {!Object} keygenAlgorithm An |AlgorithmIdentifier| to be used as
 * parameter to |RTCPeerConnection.generateCertificate|. The resulting
 * certificate will be used by the peer connection.
 */
function generateAndCloneCertificate(keygenAlgorithm) {
  return RTCPeerConnection.generateCertificate(keygenAlgorithm).then(
      function(certificate) {
        gCertificate = certificate;
        if (gCertificate.getFingerprints().length == 0)
          throw new Error('getFingerprints() is empty.');
        for (let i = 0; i < gCertificate.getFingerprints().length; ++i) {
          if (gCertificate.getFingerprints()[i].algorithm != 'sha-256')
            throw new Error('Unexpected fingerprint algorithm.');
          if (gCertificate.getFingerprints()[i].value.length == 0)
            throw new Error('Unexpected fingerprint value.');
        }
        return cloneCertificate_(gCertificate).then(
            function(clone) {
              let cloneIsEqual = (clone.getFingerprints().length ==
                                  gCertificate.getFingerprints().length);
              if (cloneIsEqual) {
                for (let i = 0; i < clone.getFingerprints().length; ++i) {
                  if (clone.getFingerprints()[i].algorithm !=
                      gCertificate.getFingerprints()[i].algorithm ||
                      clone.getFingerprints()[i].value !=
                      gCertificate.getFingerprints()[i].value) {
                    cloneIsEqual = false;
                    break;
                  }
                }
              }
              if (!cloneIsEqual) {
                throw new Error('The cloned certificate\'s fingerprints does ' +
                               'not match the original certificate.');
              }

              gCertificateClone = clone;
              return logAndReturn('ok-generated-and-cloned');
            },
            function() {
              throw new Error('Error cloning certificate.');
            });
      },
      function() {
        throw new Error('Certificate generation failed. keygenAlgorithm: ' +
            JSON.stringify(keygenAlgorithm));
      });
}

// Internals.

/** @private */
function saveCertificate_(certificate) {
  return new Promise(function(resolve, reject) {
    if (gDatabase === null)
      throw new Error('The database is not open.');

    var certTrans = gDatabase.transaction('certificates', 'readwrite');
    var certStore = certTrans.objectStore('certificates');
    var certPut = certStore.put({
      id:0,
      cert:certificate
    });

    certPut.onsuccess = function() {
      resolve();
    };
    certPut.onerror = function() {
      reject(certPut.error);
    };
  });
}

/** @private */
function loadCertificate_() {
  return new Promise(function(resolve, reject) {
    if (gDatabase === null)
      throw new Error('The database is not open.');

    var certTrans = gDatabase.transaction('certificates', 'readonly');
    var certStore = certTrans.objectStore('certificates');

    var reqGet = certStore.get(0);
    reqGet.onsuccess = function() {
      var match = reqGet.result;
      if (match !== undefined) {
        resolve(match.cert);
      } else {
        resolve(null);
      }
    };
    reqGet.onerror = function() {
      reject(reqGet.error);
    };
  });
}

/** @private */
function cloneCertificate_(certificate) {
  return saveCertificate_(certificate)
    .then(loadCertificate_)
    .then(function(clone) {
      // Save + load successful.
      if (clone === null)
        new Error('loadCertificate returned a null certificate.');
      return clone;
    });
}
