// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;

import org.chromium.base.ObserverList;

/**
 * A wrapper for DataSharingService.Observer
 *
 * <p>Hosts all the Java observers of the service. Receives all the notifications from the native
 * counterpart GroupDataObserverBridge and then notifies all the Java observers. NOTE: This observer
 * is not registered to the Java DataSharingService, this implements the DataSharingService.Observer
 * only for readability. The native observer is registered to the native service.
 */
public class ObserverBridge implements DataSharingService.Observer {
    ObserverList<DataSharingService.Observer> mJavaObservers = new ObserverList<>();

    public ObserverBridge() {}

    /** Add a new observer. Each observer can be added only once */
    public void addObserver(DataSharingService.Observer observer) {
        mJavaObservers.addObserver(observer);
    }

    /** Remove an added observer. Ignores if an observer is not in the list. */
    public void removeObserver(DataSharingService.Observer observer) {
        mJavaObservers.removeObserver(observer);
    }

    @CalledByNative
    @Override
    public void onGroupChanged(GroupData groupData) {
        for (DataSharingService.Observer javaObserver : mJavaObservers) {
            javaObserver.onGroupChanged(groupData);
        }
    }

    @CalledByNative
    @Override
    public void onGroupAdded(GroupData groupData) {
        for (DataSharingService.Observer javaObserver : mJavaObservers) {
            javaObserver.onGroupAdded(groupData);
        }
    }

    @CalledByNative
    @Override
    public void onGroupRemoved(String groupId) {
        for (DataSharingService.Observer javaObserver : mJavaObservers) {
            javaObserver.onGroupRemoved(groupId);
        }
    }
}
